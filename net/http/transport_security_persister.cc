// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/transport_security_persister.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"

namespace net {

BASE_FEATURE(kTransportSecurityFileWriterSchedule,
             "TransportSecurityFileWriterSchedule",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// From kDefaultCommitInterval in base/files/important_file_writer.cc.
// kTransportSecurityFileWriterScheduleCommitInterval won't set the commit
// interval to less than this, for performance.
constexpr base::TimeDelta kMinCommitInterval = base::Seconds(10);

// Max safe commit interval for the ImportantFileWriter.
constexpr base::TimeDelta kMaxCommitInterval = base::Minutes(10);

// Overrides the default commit interval for the ImportantFileWriter.
const base::FeatureParam<base::TimeDelta> kCommitIntervalParam(
    &kTransportSecurityFileWriterSchedule,
    "commit_interval",
    kMinCommitInterval);

constexpr const char* kHistogramSuffix = "TransportSecurityPersister";

// This function converts the binary hashes to a base64 string which we can
// include in a JSON file.
std::string HashedDomainToExternalString(
    const TransportSecurityState::HashedHost& hashed) {
  return base::Base64Encode(hashed);
}

// This inverts |HashedDomainToExternalString|, above. It turns an external
// string (from a JSON file) into an internal (binary) array.
std::optional<TransportSecurityState::HashedHost> ExternalStringToHashedDomain(
    const std::string& external) {
  TransportSecurityState::HashedHost out;
  std::optional<std::vector<uint8_t>> hashed = base::Base64Decode(external);
  if (!hashed.has_value() || hashed.value().size() != out.size()) {
    return std::nullopt;
  }

  std::copy_n(hashed.value().begin(), out.size(), out.begin());
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
// entries, respectively. The Expect-CT key is legacy and deleted when read.
const char kSTSKey[] = "sts";
const char kExpectCTKey[] = "expect_ct";

// Hostname entry, used in serialized STS dictionaries. Value is produced by
// passing hashed hostname strings to HashedDomainToExternalString().
const char kHostname[] = "host";

// Key values in serialized STS entries.
const char kStsIncludeSubdomains[] = "sts_include_subdomains";
const char kStsObserved[] = "sts_observed";
const char kExpiry[] = "expiry";
const char kMode[] = "mode";

// Values for "mode" used in serialized STS entries.
const char kForceHTTPS[] = "force-https";
const char kDefault[] = "default";

std::string LoadState(const base::FilePath& path) {
  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    return "";
  }
  return result;
}

// Serializes STS data from |state| to a Value.
base::Value::List SerializeSTSData(const TransportSecurityState* state) {
  base::Value::List sts_list;

  TransportSecurityState::STSStateIterator sts_iterator(*state);
  for (; sts_iterator.HasNext(); sts_iterator.Advance()) {
    const TransportSecurityState::STSState& sts_state =
        sts_iterator.domain_state();

    base::Value::Dict serialized;
    serialized.Set(kHostname,
                   HashedDomainToExternalString(sts_iterator.hostname()));
    serialized.Set(kStsIncludeSubdomains, sts_state.include_subdomains);
    serialized.Set(kStsObserved,
                   sts_state.last_observed.InSecondsFSinceUnixEpoch());
    serialized.Set(kExpiry, sts_state.expiry.InSecondsFSinceUnixEpoch());

    switch (sts_state.upgrade_mode) {
      case TransportSecurityState::STSState::MODE_FORCE_HTTPS:
        serialized.Set(kMode, kForceHTTPS);
        break;
      case TransportSecurityState::STSState::MODE_DEFAULT:
        serialized.Set(kMode, kDefault);
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
    const base::Value::Dict* sts_dict = sts_entry.GetIfDict();
    if (!sts_dict)
      continue;

    const std::string* hostname = sts_dict->FindString(kHostname);
    std::optional<bool> sts_include_subdomains =
        sts_dict->FindBool(kStsIncludeSubdomains);
    std::optional<double> sts_observed = sts_dict->FindDouble(kStsObserved);
    std::optional<double> expiry = sts_dict->FindDouble(kExpiry);
    const std::string* mode = sts_dict->FindString(kMode);

    if (!hostname || !sts_include_subdomains.has_value() ||
        !sts_observed.has_value() || !expiry.has_value() || !mode) {
      continue;
    }

    TransportSecurityState::STSState sts_state;
    sts_state.include_subdomains = *sts_include_subdomains;
    sts_state.last_observed =
        base::Time::FromSecondsSinceUnixEpoch(*sts_observed);
    sts_state.expiry = base::Time::FromSecondsSinceUnixEpoch(*expiry);

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

    std::optional<TransportSecurityState::HashedHost> hashed =
        ExternalStringToHashedDomain(*hostname);
    if (!hashed.has_value())
      continue;

    state->AddOrUpdateEnabledSTSHosts(hashed.value(), sts_state);
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
    const scoped_refptr<base::SequencedTaskRunner>& background_runner,
    const base::FilePath& data_path)
    : transport_security_state_(state),
      writer_(data_path,
              background_runner,
              GetCommitInterval(),
              kHistogramSuffix),
      foreground_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      background_runner_(background_runner) {
  transport_security_state_->SetDelegate(this);

  background_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadState, writer_.path()),
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
  std::optional<std::string> data = SerializeData();
  if (data) {
    writer_.WriteNow(std::move(data).value());
  } else {
    writer_.WriteNow(std::string());
  }
}

void TransportSecurityPersister::OnWriteFinished(base::OnceClosure callback) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());
  std::move(callback).Run();
}

std::optional<std::string> TransportSecurityPersister::SerializeData() {
  CHECK(foreground_runner_->RunsTasksInCurrentSequence());

  base::Value::Dict toplevel;
  toplevel.Set(kVersionKey, kCurrentVersionValue);
  toplevel.Set(kSTSKey, SerializeSTSData(transport_security_state_));

  std::string output;
  if (!base::JSONWriter::Write(toplevel, &output)) {
    return std::nullopt;
  }
  return output;
}

void TransportSecurityPersister::LoadEntries(const std::string& serialized) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  transport_security_state_->ClearDynamicData();
  bool contains_legacy_expect_ct_data = false;
  Deserialize(serialized, transport_security_state_,
              contains_legacy_expect_ct_data);
  if (contains_legacy_expect_ct_data) {
    StateIsDirty(transport_security_state_);
  }
}

// static
base::TimeDelta TransportSecurityPersister::GetCommitInterval() {
  return std::clamp(kCommitIntervalParam.Get(), kMinCommitInterval,
                    kMaxCommitInterval);
}

void TransportSecurityPersister::Deserialize(
    const std::string& serialized,
    TransportSecurityState* state,
    bool& contains_legacy_expect_ct_data) {
  std::optional<base::Value> value = base::JSONReader::Read(serialized);
  if (!value || !value->is_dict())
    return;

  base::Value::Dict& dict = value->GetDict();
  std::optional<int> version = dict.FindInt(kVersionKey);

  // Stop if the data is out of date (or in the previous format that didn't have
  // a version number).
  if (!version || *version != kCurrentVersionValue)
    return;

  base::Value* sts_value = dict.Find(kSTSKey);
  if (sts_value)
    DeserializeSTSData(*sts_value, state);

  // If an Expect-CT key is found on deserialization, record this so that a
  // write can be scheduled to clear it from disk.
  contains_legacy_expect_ct_data = !!dict.Find(kExpectCTKey);
}

void TransportSecurityPersister::CompleteLoad(const std::string& state) {
  DCHECK(foreground_runner_->RunsTasksInCurrentSequence());

  if (state.empty())
    return;

  LoadEntries(state);
}

}  // namespace net
