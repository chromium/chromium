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
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"

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

// Obsolete values in older STS entries.
const char kIncludeSubdomains[] = "include_subdomains";
const char kStrict[] = "strict";
const char kPinningOnly[] = "pinning-only";
const char kCreated[] = "created";
const char kExpectCTSubdictionary[] = "expect_ct";

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
    base::Optional<bool> sts_include_subdomains =
        sts_entry.FindBoolKey(kStsIncludeSubdomains);
    base::Optional<double> sts_observed = sts_entry.FindDoubleKey(kStsObserved);
    base::Optional<double> expiry = sts_entry.FindDoubleKey(kExpiry);
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
    base::Optional<double> expect_ct_last_observed =
        ct_entry.FindDoubleKey(kExpectCTObserved);
    base::Optional<double> expect_ct_expiry =
        ct_entry.FindDoubleKey(kExpectCTExpiry);
    base::Optional<bool> expect_ct_enforce =
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

// Handles deserializing |kExpectCTSubdictionary| in dictionaries that use the
// obsolete format. Populates |state| with the values from the Expect-CT
// subdictionary in |parsed|. Returns false if |parsed| is malformed (e.g.
// missing a required Expect-CT key) and true otherwise. Note that true does not
// necessarily mean that Expect-CT state was present in |parsed|.
//
// TODO(mmenke): Remove once the obsolete format is no longer supported.
bool DeserializeObsoleteExpectCTState(
    const base::Value* parsed,
    TransportSecurityState::ExpectCTState* state) {
  const base::Value* expect_ct_subdictionary =
      parsed->FindDictKey(kExpectCTSubdictionary);
  if (!expect_ct_subdictionary) {
    // Expect-CT data is not required, so this item is not malformed.
    return true;
  }
  base::Optional<double> observed =
      expect_ct_subdictionary->FindDoubleKey(kExpectCTObserved);
  base::Optional<double> expiry =
      expect_ct_subdictionary->FindDoubleKey(kExpectCTExpiry);
  base::Optional<bool> enforce =
      expect_ct_subdictionary->FindBoolKey(kExpectCTEnforce);
  const std::string* report_uri_str =
      expect_ct_subdictionary->FindStringKey(kExpectCTReportUri);

  // If an Expect-CT subdictionary is present, it must have the required keys.
  if (!observed.has_value() || !expiry.has_value() || !enforce.has_value())
    return false;

  state->last_observed = base::Time::FromDoubleT(*observed);
  state->expiry = base::Time::FromDoubleT(*expiry);
  state->enforce = *enforce;
  if (report_uri_str) {
    GURL report_uri(*report_uri_str);
    if (report_uri.is_valid())
      state->report_uri = report_uri;
  }
  return true;
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

bool TransportSecurityPersister::LoadEntries(const std::string& serialized,
                                             bool* data_in_old_format) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  transport_security_state_->ClearDynamicData();
  return Deserialize(serialized, data_in_old_format, transport_security_state_);
}

bool TransportSecurityPersister::Deserialize(const std::string& serialized,
                                             bool* data_in_old_format,
                                             TransportSecurityState* state) {
  *data_in_old_format = false;
  base::Optional<base::Value> value = base::JSONReader::Read(serialized);
  if (!value || !value->is_dict())
    return false;

  // Old dictionaries don't have a version number, so try the obsolete format if
  // there's no integer version number.
  base::Optional<int> version = value->FindIntKey(kVersionKey);
  if (!version) {
    bool dirty_unused = false;
    bool success = DeserializeObsoleteData(*value, &dirty_unused, state);
    // If successfully loaded data from a file in the old format, need to
    // overwrite the file with the newer format.
    *data_in_old_format = success;
    return success;
  }

  if (*version != kCurrentVersionValue)
    return false;

  base::Value* sts_value = value->FindKey(kSTSKey);
  if (sts_value)
    DeserializeSTSData(*sts_value, state);

  base::Value* expect_ct_value = value->FindKey(kExpectCTKey);
  if (expect_ct_value)
    DeserializeExpectCTData(*expect_ct_value, state);

  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ExpectCT.EntriesOnLoad",
                              state->num_expect_ct_entries(), 1 /* min */,
                              2000 /* max */, 40 /* buckets */);
  return true;
}

bool TransportSecurityPersister::DeserializeObsoleteData(
    const base::Value& dict_value,
    bool* dirty,
    TransportSecurityState* state) {
  const base::Time current_time(base::Time::Now());
  bool dirtied = false;

  // The one caller ensures |dict_value| is of Value::Type::DICTIONARY already.
  DCHECK(dict_value.is_dict());

  for (const auto& i : dict_value.DictItems()) {
    const base::Value& parsed = i.second;
    if (!parsed.is_dict()) {
      LOG(WARNING) << "Could not parse entry " << i.first << "; skipping entry";
      continue;
    }

    TransportSecurityState::STSState sts_state;
    TransportSecurityState::ExpectCTState expect_ct_state;

    // kIncludeSubdomains is a legacy synonym for kStsIncludeSubdomains. Parse
    // at least one of these properties, preferably the new one.
    bool parsed_include_subdomains = false;
    base::Optional<bool> include_subdomains =
        parsed.FindBoolKey(kIncludeSubdomains);
    if (include_subdomains.has_value()) {
      sts_state.include_subdomains = include_subdomains.value();
      parsed_include_subdomains = true;
    }
    include_subdomains = parsed.FindBoolKey(kStsIncludeSubdomains);
    if (include_subdomains.has_value()) {
      sts_state.include_subdomains = include_subdomains.value();
      parsed_include_subdomains = true;
    }

    const std::string* mode_string = parsed.FindStringKey(kMode);
    base::Optional<double> expiry = parsed.FindDoubleKey(kExpiry);  // 0;
    if (!parsed_include_subdomains || !mode_string || !expiry.has_value()) {
      LOG(WARNING) << "Could not parse some elements of entry " << i.first
                   << "; skipping entry";
      continue;
    }

    if (*mode_string == kForceHTTPS || *mode_string == kStrict) {
      sts_state.upgrade_mode =
          TransportSecurityState::STSState::MODE_FORCE_HTTPS;
    } else if (*mode_string == kDefault || *mode_string == kPinningOnly) {
      sts_state.upgrade_mode = TransportSecurityState::STSState::MODE_DEFAULT;
    } else {
      LOG(WARNING) << "Unknown TransportSecurityState mode string "
                   << mode_string << " found for entry " << i.first
                   << "; skipping entry";
      continue;
    }

    sts_state.expiry = base::Time::FromDoubleT(expiry.value());

    base::Optional<double> sts_observed = parsed.FindDoubleKey(kStsObserved);
    if (sts_observed.has_value()) {
      sts_state.last_observed = base::Time::FromDoubleT(sts_observed.value());
    } else if (parsed.FindDoubleKey(kCreated)) {
      // kCreated is a legacy synonym for both kStsObserved.
      sts_observed = parsed.FindDoubleKey(kCreated);
      sts_state.last_observed = base::Time::FromDoubleT(sts_observed.value());
    } else {
      // We're migrating an old entry with no observation date. Make sure we
      // write the new date back in a reasonable time frame.
      dirtied = true;
      sts_state.last_observed = base::Time::Now();
    }

    if (!DeserializeObsoleteExpectCTState(&parsed, &expect_ct_state)) {
      continue;
    }

    bool has_sts =
        sts_state.expiry > current_time && sts_state.ShouldUpgradeToSSL();
    bool has_expect_ct =
        expect_ct_state.expiry > current_time &&
        (expect_ct_state.enforce || !expect_ct_state.report_uri.is_empty());
    if (!has_sts && !has_expect_ct) {
      // Make sure we dirty the state if we drop an entry. The entries can only
      // be dropped when all the STS and Expect-CT states are expired or
      // invalid.
      dirtied = true;
      continue;
    }

    std::string hashed = ExternalStringToHashedDomain(i.first);
    if (hashed.empty()) {
      dirtied = true;
      continue;
    }

    // Until the on-disk storage is split, there will always be 'null' entries.
    // We only register entries that have actual state.
    if (has_sts)
      state->AddOrUpdateEnabledSTSHosts(hashed, sts_state);
    if (has_expect_ct) {
      // Use empty NetworkIsolationKeys for old data.
      state->AddOrUpdateEnabledExpectCTHosts(hashed, NetworkIsolationKey(),
                                             expect_ct_state);
    }
  }

  *dirty = dirtied;
  return true;
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  if (state.empty())
    return;

  bool data_in_old_format = false;
  if (!LoadEntries(state, &data_in_old_format))
    return;
  if (data_in_old_format)
    StateIsDirty(transport_security_state_);
}

}  // namespace net
