// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_persister.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

// This function converts the binary hashes to a base64 string which we can
// include in a JSON file.
std::string HashedDomainToExternalString(const std::string& hashed) {
  std::string out;
  base::Base64Encode(hashed, &out);
  return out;
}

// This inverts |HashedDomainToExternalString|, above. It turns an external
// string (from a JSON file) into an internal (binary) string.
std::string ExternalStringToHashedDomain(const std::string& external) {
  std::string out;
  if (!base::Base64Decode(external, &out) ||
      out.size() != crypto::kSHA256Length) {
    return std::string();
  }

  return out;
}

// Version 2 of the on-disk format consists of a single JSON object. The
// top-level dictionary has "version", "sts", and "expect_ct" entries. The first
// is an integer, the latter two are unordered lists of dictionaries, each
// representing cached data for a single host.

// Stored in serialized dictionary values to distinguish incompatible versions.
// Version 1 is distinguished by the lack of an integer version value.
const char kVersionKey[] = "version";
const int kCurrentVersionValue = 2;

// Keys in top level serialized dictionary, for lists of STS and Expect-CT
// entries, respectively.
const char kSTSKey[] = "sts";
const char kExpectCTKey[] = "expect_ct";

// Hostname entry, used in serialized STS and Expect-CT dictionaries. Value is
// produced by passing hashed hostname strings to
// HashedDomainToExternalString().
const char kHostname[] = "host";

// Key values in serialized STS entries.
const char kStsIncludeSubdomains[] = "sts_include_subdomains";
const char kStsObserved[] = "sts_observed";
const char kExpiry[] = "expiry";
const char kMode[] = "mode";

// Values for "mode" used in serialized STS entries.
const char kForceHTTPS[] = "force-https";
const char kDefault[] = "default";

// Key names in serialized Expect-CT entries.
const char kNetworkIsolationKey[] = "nik";
const char kExpectCTObserved[] = "expect_ct_observed";
const char kExpectCTExpiry[] = "expect_ct_expiry";
const char kExpectCTEnforce[] = "expect_ct_enforce";
const char kExpectCTReportUri[] = "expect_ct_report_uri";

std::string LoadState(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    return "";
  }
  return result;
}

bool IsDynamicExpectCTEnabled() {
  return base::FeatureList::IsEnabled(
      TransportSecurityState::kDynamicExpectCTFeature);
}

// Serializes STS data from |state| to a Value.
base::Value SerializeSTSData(const TransportSecurityState* state) {
  base::Value sts_list(base::Value::Type::LIST);

  TransportSecurityState::STSStateIterator sts_iterator(*state);
  for (; sts_iterator.HasNext(); sts_iterator.Advance()) {
    const TransportSecurityState::STSState& sts_state =
        sts_iterator.domain_state();

    base::Value serialized(base::Value::Type::DICTIONARY);
    serialized.SetStringKey(
        kHostname, HashedDomainToExternalString(sts_iterator.hostname()));
    serialized.SetBoolKey(kStsIncludeSubdomains, sts_state.include_subdomains);
    serialized.SetDoubleKey(kStsObserved, sts_state.last_observed.ToDoubleT());
    serialized.SetDoubleKey(kExpiry, sts_state.expiry.ToDoubleT());

    switch (sts_state.upgrade_mode) {
      case TransportSecurityState::STSState::MODE_FORCE_HTTPS:
        serialized.SetStringKey(kMode, kForceHTTPS);
        break;
      case TransportSecurityState::STSState::MODE_DEFAULT:
        serialized.SetStringKey(kMode, kDefault);
        break;
    }

    sts_list.Append(std::move(serialized));
  }
  return sts_list;
}

// Deserializes STS data from a Value created by the above method.
void DeserializeSTSData(const base::Value& sts_list,
                        TransportSecurityState* state) {
  if (!sts_list.is_list())
    return;

  base::Time current_time(base::Time::Now());

  for (const base::Value& sts_entry : sts_list.GetList()) {
    if (!sts_entry.is_dict())
      continue;

    const std::string* hostname = sts_entry.FindStringKey(kHostname);
    absl::optional<bool> sts_include_subdomains =
        sts_entry.FindBoolKey(kStsIncludeSubdomains);
    absl::optional<double> sts_observed = sts_entry.FindDoubleKey(kStsObserved);
    absl::optional<double> expiry = sts_entry.FindDoubleKey(kExpiry);
    const std::string* mode = sts_entry.FindStringKey(kMode);

    if (!hostname || !sts_include_subdomains.has_value() ||
        !sts_observed.has_value() || !expiry.has_value() || !mode) {
      continue;
    }

    TransportSecurityState::STSState sts_state;
    sts_state.include_subdomains = *sts_include_subdomains;
    sts_state.last_observed = base::Time::FromDoubleT(*sts_observed);
    sts_state.expiry = base::Time::FromDoubleT(*expiry);

    if (*mode == kForceHTTPS) {
      sts_state.upgrade_mode =
          TransportSecurityState::STSState::MODE_FORCE_HTTPS;
    } else if (*mode == kDefault) {
      sts_state.upgrade_mode = TransportSecurityState::STSState::MODE_DEFAULT;
    } else {
      continue;
    }

    if (sts_state.expiry < current_time || !sts_state.ShouldUpgradeToSSL())
      continue;

    std::string hashed = ExternalStringToHashedDomain(*hostname);
    if (hashed.empty())
      continue;

    state->AddOrUpdateEnabledSTSHosts(hashed, sts_state);
  }
}

// Serializes Expect-CT data from |state| to a Value.
base::Value SerializeExpectCTData(TransportSecurityState* state) {
  base::Value ct_list(base::Value::Type::LIST);

  if (!IsDynamicExpectCTEnabled())
    return ct_list;

  TransportSecurityState::ExpectCTStateIterator expect_ct_iterator(*state);
  for (; expect_ct_iterator.HasNext(); expect_ct_iterator.Advance()) {
    const TransportSecurityState::ExpectCTState& expect_ct_state =
        expect_ct_iterator.domain_state();

    base::Value ct_entry(base::Value::Type::DICTIONARY);

    base::Value network_isolation_key_value;
    // Don't serialize entries with transient NetworkIsolationKeys.
    if (!expect_ct_iterator.network_isolation_key().ToValue(
            &network_isolation_key_value)) {
      continue;
    }
    ct_entry.SetKey(kNetworkIsolationKey,
                    std::move(network_isolation_key_value));

    ct_entry.SetStringKey(
        kHostname, HashedDomainToExternalString(expect_ct_iterator.hostname()));
    ct_entry.SetDoubleKey(kExpectCTObserved,
                          expect_ct_state.last_observed.ToDoubleT());
    ct_entry.SetDoubleKey(kExpectCTExpiry, expect_ct_state.expiry.ToDoubleT());
    ct_entry.SetBoolKey(kExpectCTEnforce, expect_ct_state.enforce);
    ct_entry.SetStringKey(kExpectCTReportUri,
                          expect_ct_state.report_uri.spec());

    ct_list.Append(std::move(ct_entry));
  }

  return ct_list;
}

// Deserializes Expect-CT data from a Value created by the above method.
void DeserializeExpectCTData(const base::Value& ct_list,
                             TransportSecurityState* state) {
  if (!ct_list.is_list())
    return;
  bool partition_by_nik = base::FeatureList::IsEnabled(
      features::kPartitionExpectCTStateByNetworkIsolationKey);

  const base::Time current_time(base::Time::Now());

  for (const base::Value& ct_entry : ct_list.GetList()) {
    if (!ct_entry.is_dict())
      continue;

    const std::string* hostname = ct_entry.FindStringKey(kHostname);
    const base::Value* network_isolation_key_value =
        ct_entry.FindKey(kNetworkIsolationKey);
    absl::optional<double> expect_ct_last_observed =
        ct_entry.FindDoubleKey(kExpectCTObserved);
    absl::optional<double> expect_ct_expiry =
        ct_entry.FindDoubleKey(kExpectCTExpiry);
    absl::optional<bool> expect_ct_enforce =
        ct_entry.FindBoolKey(kExpectCTEnforce);
    const std::string* expect_ct_report_uri =
        ct_entry.FindStringKey(kExpectCTReportUri);

    if (!hostname || !network_isolation_key_value ||
        !expect_ct_last_observed.has_value() || !expect_ct_expiry.has_value() ||
        !expect_ct_enforce.has_value() || !expect_ct_report_uri) {
      continue;
    }

    TransportSecurityState::ExpectCTState expect_ct_state;
    expect_ct_state.last_observed =
        base::Time::FromDoubleT(*expect_ct_last_observed);
    expect_ct_state.expiry = base::Time::FromDoubleT(*expect_ct_expiry);
    expect_ct_state.enforce = *expect_ct_enforce;

    GURL report_uri(*expect_ct_report_uri);
    if (report_uri.is_valid())
      expect_ct_state.report_uri = report_uri;

    if (expect_ct_state.expiry < current_time ||
        (!expect_ct_state.enforce && expect_ct_state.report_uri.is_empty())) {
      continue;
    }

    std::string hashed = ExternalStringToHashedDomain(*hostname);
    if (hashed.empty())
      continue;

    NetworkIsolationKey network_isolation_key;
    if (!NetworkIsolationKey::FromValue(*network_isolation_key_value,
                                        &network_isolation_key)) {
      continue;
    }

    // If Expect-CT is not being partitioned by NetworkIsolationKey, but
    // |network_isolation_key| is not empty, drop the entry, to avoid ambiguity
    // and favor entries that were saved with an empty NetworkIsolationKey.
    if (!partition_by_nik && !network_isolation_key.IsEmpty())
      continue;

    state->AddOrUpdateEnabledExpectCTHosts(hashed, network_isolation_key,
                                           expect_ct_state);
  }
}

void OnWriteFinishedTask(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         base::OnceClosure callback,
                         bool result) {
  task_runner->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

TransportSecurityPersister::TransportSecurityPersister(
    TransportSecurityState* state,
    const base::FilePath& profile_path,
    const scoped_refptr<base::SequencedTaskRunner>& background_runner)
    : transport_security_state_(state),
      writer_(profile_path.AppendASCII("TransportSecurity"), background_runner),
      foreground_runner_(base::ThreadTaskRunnerHandle::Get()),
      background_runner_(background_runner) {
  transport_security_state_->SetDelegate(this);

  base::PostTaskAndReplyWithResult(
      background_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadState, writer_.path()),
      base::BindOnce(&TransportSecurityPersister::CompleteLoad,
                     weak_ptr_factory_.GetWeakPtr()));
}

TransportSecurityPersister::~TransportSecurityPersister() {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  if (writer_.HasPendingWrite())
    writer_.DoScheduledWrite();

  transport_security_state_->SetDelegate(nullptr);
}

void TransportSecurityPersister::StateIsDirty(TransportSecurityState* state) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(transport_security_state_, state);

  writer_.ScheduleWrite(this);
}

void TransportSecurityPersister::WriteNow(TransportSecurityState* state,
                                          base::OnceClosure callback) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(transport_security_state_, state);

  writer_.RegisterOnNextWriteCallbacks(
      base::OnceClosure(),
      base::BindOnce(
          &OnWriteFinishedTask, foreground_runner_,
          base::BindOnce(&TransportSecurityPersister::OnWriteFinished,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
  auto data = std::make_unique<std::string>();
  SerializeData(data.get());
  writer_.WriteNow(std::move(data));
}

void TransportSecurityPersister::OnWriteFinished(base::OnceClosure callback) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());
  std::move(callback).Run();
}

bool TransportSecurityPersister::SerializeData(std::string* output) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  base::Value toplevel(base::Value::Type::DICTIONARY);
  toplevel.SetIntKey(kVersionKey, kCurrentVersionValue);
  toplevel.SetKey(kSTSKey, SerializeSTSData(transport_security_state_));
  toplevel.SetKey(kExpectCTKey,
                  SerializeExpectCTData(transport_security_state_));

  base::JSONWriter::Write(toplevel, output);
  return true;
}

void TransportSecurityPersister::LoadEntries(const std::string& serialized) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  transport_security_state_->ClearDynamicData();
  Deserialize(serialized, transport_security_state_);
}

void TransportSecurityPersister::Deserialize(const std::string& serialized,
                                             TransportSecurityState* state) {
  absl::optional<base::Value> value = base::JSONReader::Read(serialized);
  if (!value || !value->is_dict())
    return;

  absl::optional<int> version = value->FindIntKey(kVersionKey);

  // Stop if the data is out of date (or in the previous format that didn't have
  // a version number).
  if (!version || *version != kCurrentVersionValue)
    return;

  base::Value* sts_value = value->FindKey(kSTSKey);
  if (sts_value)
    DeserializeSTSData(*sts_value, state);

  base::Value* expect_ct_value = value->FindKey(kExpectCTKey);
  if (expect_ct_value)
    DeserializeExpectCTData(*expect_ct_value, state);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ExpectCT.EntriesOnLoad",
                              state->num_expect_ct_entries(), 1 /* min */,
                              2000 /* max */, 40 /* buckets */);
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  if (state.empty())
    return;

  LoadEntries(state);
}

}  // namespace net
