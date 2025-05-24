// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"

#import "base/json/values_util.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

// Threshold for deleting files.
constexpr base::TimeDelta kFileDeletionThreshold = base::Days(30);

// Creates a ScheduledFile from the given base::Value.
std::optional<auto_deletion::ScheduledFile> ScheduledFileFromValue(
    const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& dict = value.GetDict();
  const base::Value* time_value = dict.Find("time");
  if (!time_value) {
    return std::nullopt;
  }

  const std::optional<base::Time> maybe_time = base::ValueToTime(*time_value);
  if (!maybe_time) {
    return std::nullopt;
  }

  const std::string* maybe_hash = dict.FindString("hash");
  if (!maybe_hash) {
    return std::nullopt;
  }

  const std::string* maybe_path = dict.FindString("path");
  if (!maybe_path) {
    return std::nullopt;
  }

  return auto_deletion::ScheduledFile(base::FilePath(*maybe_path), *maybe_hash,
                                      *maybe_time);
}

}  // namespace

namespace auto_deletion {

Scheduler::Scheduler(PrefService* local_state) : local_state_(local_state) {
  DCHECK(local_state_);
}

Scheduler::~Scheduler() = default;

std::vector<ScheduledFile> Scheduler::IdentifyExpiredFiles(base::Time instant) {
  std::vector<ScheduledFile> expired_files;
  const base::Value::List& files =
      local_state_->GetList(prefs::kDownloadAutoDeletionScheduledFiles);

  for (const auto& value : files) {
    std::optional<ScheduledFile> maybe_file = ScheduledFileFromValue(value);
    if (!maybe_file) {
      continue;
    }

    if (!IsFileReadyForDeletion(instant, *maybe_file)) {
      break;
    }

    expired_files.push_back(std::move(*maybe_file));
  }

  return expired_files;
}

void Scheduler::RemoveExpiredFiles(base::Time instant) {
  ScopedListPrefUpdate update(local_state_,
                              prefs::kDownloadAutoDeletionScheduledFiles);

  unsigned int values_to_erase_count = 0;
  for (const auto& value : update.Get()) {
    std::optional<ScheduledFile> maybe_file = ScheduledFileFromValue(value);
    if (!maybe_file) {
      ++values_to_erase_count;
      continue;
    }

    if (!IsFileReadyForDeletion(instant, *maybe_file)) {
      break;
    }

    ++values_to_erase_count;
  }

  if (values_to_erase_count > 0) {
    base::Value::List& value = update.Get();
    value.erase(value.begin(), value.begin() + values_to_erase_count);
  }
}

void Scheduler::ScheduleFile(ScheduledFile file) {
  ScopedListPrefUpdate update(local_state_,
                              prefs::kDownloadAutoDeletionScheduledFiles);
  update->Append(base::Value::Dict()
                     .Set("path", file.filepath().AsUTF8Unsafe())
                     .Set("hash", file.hash())
                     .Set("time", base::TimeToValue(file.download_time())));
}

void Scheduler::Clear() {
  local_state_->ClearPref(prefs::kDownloadAutoDeletionScheduledFiles);
}

bool Scheduler::IsFileReadyForDeletion(base::Time instant,
                                       const ScheduledFile& file) {
  const base::Time download_date = file.download_time();
  const base::TimeDelta download_age = instant - download_date;
  return download_age > kFileDeletionThreshold;
}

}  // namespace auto_deletion
