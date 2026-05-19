// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"

#import <string_view>
#import <vector>

#import "base/json/json_writer.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/memory/safe_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "url/gurl.h"

namespace {

// Logs a journal entry to the console.
void LogJournalEntry(const actor::AggregatedJournal::Entry* entry) {
  CHECK(IsActorServiceLoggingEnabled());

  if (!entry || !entry->data) {
    return;
  }

  std::string type_str;
  switch (entry->data->type) {
    case actor::JournalEntryType::kBegin:
      type_str = "Begin";
      break;
    case actor::JournalEntryType::kEnd:
      type_str = "End";
      break;
    case actor::JournalEntryType::kInstant:
      type_str = "Instant";
      break;
  }

  std::string details_str;
  for (const auto& detail : entry->data->details) {
    details_str += detail.key + ": " + detail.value + ", ";
  }

  if (!details_str.empty()) {
    details_str = " [" + details_str.substr(0, details_str.length() - 2) + "]";
  }

  LOG(INFO) << "ActorService::AggregatedJournal: [" << type_str << "] Task "
            << entry->data->task_id.value() << " - " << entry->data->event
            << details_str;
}

}  // namespace

namespace actor {

JournalEntry::JournalEntry() = default;
JournalEntry::JournalEntry(const JournalEntry&) = default;
JournalEntry& JournalEntry::operator=(const JournalEntry&) = default;
JournalEntry::~JournalEntry() = default;

AggregatedJournal::Entry::Entry(const std::string& location,
                                std::unique_ptr<JournalEntry> data)
    : url(location), data(std::move(data)) {}
AggregatedJournal::Entry::~Entry() = default;

AggregatedJournal::PendingAsyncEntry::PendingAsyncEntry(
    base::PassKey<AggregatedJournal>,
    base::WeakPtr<AggregatedJournal> journal,
    ActorTaskId task_id,
    std::string_view event_name,
    uint64_t track_uuid)
    : journal_(std::move(journal)),
      task_id_(task_id),
      event_name_(event_name),
      begin_time_(base::TimeTicks::Now()),
      track_uuid_(track_uuid) {}

AggregatedJournal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!terminated_) {
    EndEntry({});
  }
}

void AggregatedJournal::PendingAsyncEntry::EndEntry(
    std::vector<JournalDetails> details) {
  if (terminated_ || !journal_) {
    return;
  }
  terminated_ = true;
  journal_->AddEndEvent(base::PassKey<AggregatedJournal>(), task_id_,
                        event_name_, track_uuid_, std::move(details));
}

AggregatedJournal* AggregatedJournal::PendingAsyncEntry::GetJournal() {
  return journal_.get();
}

ActorTaskId AggregatedJournal::PendingAsyncEntry::GetTaskId() {
  return task_id_;
}

AggregatedJournal::AggregatedJournal() = default;
AggregatedJournal::~AggregatedJournal() = default;

uint64_t AggregatedJournal::AllocateDynamicTrackUUID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static uint64_t next_track_id = 1000;
  return ++next_track_id;
}

std::unique_ptr<AggregatedJournal::PendingAsyncEntry>
AggregatedJournal::CreatePendingAsyncEntry(
    const GURL& url,
    ActorTaskId task_id,
    uint64_t track_uuid,
    std::string_view event_name,
    std::vector<JournalDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Log the Begin event.
  std::unique_ptr<JournalEntry> entry = std::make_unique<JournalEntry>();
  entry->type = JournalEntryType::kBegin;
  entry->task_id = task_id;
  entry->timestamp = base::Time::Now();
  entry->event = std::string(event_name);
  entry->track_uuid = track_uuid;
  entry->details = std::move(details);
  AddEntry(
      std::make_unique<Entry>(url.possibly_invalid_spec(), std::move(entry)));

  return std::make_unique<PendingAsyncEntry>(base::PassKey<AggregatedJournal>(),
                                             GetWeakPtr(), task_id, event_name,
                                             track_uuid);
}

void AggregatedJournal::Log(const GURL& url,
                            ActorTaskId task_id,
                            std::string_view event_name,
                            std::vector<JournalDetails> details) {
  Log(url, task_id, 0, event_name, std::move(details));
}

void AggregatedJournal::Log(const GURL& url,
                            ActorTaskId task_id,
                            uint64_t track_uuid,
                            std::string_view event_name,
                            std::vector<JournalDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<JournalEntry> entry = std::make_unique<JournalEntry>();
  entry->type = JournalEntryType::kInstant;
  entry->task_id = task_id;
  entry->timestamp = base::Time::Now();
  entry->event = std::string(event_name);
  entry->track_uuid = track_uuid;
  entry->details = std::move(details);
  AddEntry(
      std::make_unique<Entry>(url.possibly_invalid_spec(), std::move(entry)));
}

std::vector<JournalEntry> AggregatedJournal::GetLogs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<JournalEntry> result;
  for (EntryBuffer::Iterator it = entries_.Begin(); it; ++it) {
    const std::unique_ptr<Entry>* entry_ptr = *it;
    if (entry_ptr && *entry_ptr && (*entry_ptr)->data) {
      result.push_back(*(*entry_ptr)->data);
    }
  }
  return result;
}

std::string AggregatedJournal::GetLogsAsJson() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ListValue list;
  for (EntryBuffer::Iterator it = entries_.Begin(); it; ++it) {
    const std::unique_ptr<Entry>* entry_ptr = *it;
    if (!entry_ptr || !*entry_ptr || !(*entry_ptr)->data) {
      continue;
    }
    const Entry& entry_wrapper = **entry_ptr;
    const JournalEntry& entry = *entry_wrapper.data;
    base::DictValue dict;
    switch (entry.type) {
      case JournalEntryType::kBegin:
        dict.Set("type", "Begin");
        break;
      case JournalEntryType::kEnd:
        dict.Set("type", "End");
        break;
      case JournalEntryType::kInstant:
        dict.Set("type", "Instant");
        break;
    }
    dict.Set("task_id", base::NumberToString(entry.task_id.value()));
    dict.Set("event", entry.event);
    dict.Set("timestamp", entry.timestamp.InSecondsFSinceUnixEpoch());
    dict.Set("track_uuid", base::NumberToString(entry.track_uuid));
    if (!entry_wrapper.url.empty()) {
      dict.Set("url", entry_wrapper.url);
    }

    base::ListValue details_list;
    for (const JournalDetails& detail : entry.details) {
      base::DictValue detail_dict;
      detail_dict.Set("key", detail.key);
      detail_dict.Set("value", detail.value);
      details_list.Append(std::move(detail_dict));
    }
    dict.Set("details", std::move(details_list));
    list.Append(std::move(dict));
  }

  std::string json;
  base::JSONWriter::Write(list, &json);
  return json;
}

void AggregatedJournal::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.Clear();
}

base::SafeRef<AggregatedJournal> AggregatedJournal::GetSafeRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::SafeRef<AggregatedJournal>(weak_ptr_factory_.GetSafeRef());
}

base::WeakPtr<AggregatedJournal> AggregatedJournal::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void AggregatedJournal::AddEndEvent(base::PassKey<AggregatedJournal>,
                                    ActorTaskId task_id,
                                    const std::string& event_name,
                                    uint64_t track_uuid,
                                    std::vector<JournalDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<JournalEntry> entry = std::make_unique<JournalEntry>();
  entry->type = JournalEntryType::kEnd;
  entry->task_id = task_id;
  entry->timestamp = base::Time::Now();
  entry->event = event_name;
  entry->track_uuid = track_uuid;
  entry->details = std::move(details);
  AddEntry(std::make_unique<Entry>(std::string(), std::move(entry)));
}

void AggregatedJournal::AddEntry(std::unique_ptr<Entry> entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsActorServiceLoggingEnabled()) {
    LogJournalEntry(entry.get());
  }

  entries_.SaveToBuffer(std::move(entry));
}

}  // namespace actor
