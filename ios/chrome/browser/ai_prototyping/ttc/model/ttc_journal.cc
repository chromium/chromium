// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

std::string_view TTCJournalComponentToString(TTCJournalComponent component) {
  switch (component) {
    case TTCJournalComponent::kSession:
      return "Session";
    case TTCJournalComponent::kConversation:
      return "Conv";
    case TTCJournalComponent::kBackend:
      return "Backend";
    case TTCJournalComponent::kAudio:
      return "Audio";
    case TTCJournalComponent::kActuation:
      return "Actuation";
  }
}

TTCJournalDetailsBuilder::TTCJournalDetailsBuilder() = default;
TTCJournalDetailsBuilder::TTCJournalDetailsBuilder(TTCJournalDetailsBuilder&&) =
    default;
TTCJournalDetailsBuilder& TTCJournalDetailsBuilder::operator=(
    TTCJournalDetailsBuilder&&) = default;
TTCJournalDetailsBuilder::~TTCJournalDetailsBuilder() = default;

TTCJournalEntry::TTCJournalEntry() = default;
TTCJournalEntry::TTCJournalEntry(const TTCJournalEntry&) = default;
TTCJournalEntry& TTCJournalEntry::operator=(const TTCJournalEntry&) = default;
TTCJournalEntry::TTCJournalEntry(TTCJournalEntry&&) = default;
TTCJournalEntry& TTCJournalEntry::operator=(TTCJournalEntry&&) = default;
TTCJournalEntry::~TTCJournalEntry() = default;

std::string TTCJournalEntry::FormatLine() const {
  std::string line;
  base::Time::Exploded exploded;
  timestamp.UTCExplode(&exploded);
  if (exploded.HasValidValues()) {
    base::StringAppendF(&line, "%02d:%02d:%02d.%03d ", exploded.hour,
                        exploded.minute, exploded.second, exploded.millisecond);
  }

  std::string_view component_label = TTCJournalComponentToString(component);
  base::StringAppendF(&line, "[%.*s] ",
                      static_cast<int>(component_label.size()),
                      component_label.data());

  switch (type) {
    case TTCJournalEntryType::kInstant:
      break;
    case TTCJournalEntryType::kBegin:
      line += "[Begin] ";
      break;
    case TTCJournalEntryType::kEnd:
      line += "[End] ";
      break;
    case TTCJournalEntryType::kError:
      line += "[ERROR] ";
      break;
  }

  line += event_name;

  if (!details.empty()) {
    line += " (";
    for (size_t i = 0; i < details.size(); ++i) {
      if (i > 0) {
        line += ", ";
      }
      line += details[i].key;
      line += "=";
      line += details[i].value;
    }
    line += ")";
  }

  return line;
}

TTCJournal::PendingAsyncEntry::PendingAsyncEntry(
    base::WeakPtr<TTCJournal> journal,
    TTCJournalComponent component,
    std::string_view event_name,
    base::TimeTicks begin_ticks)
    : journal_(std::move(journal)),
      component_(component),
      event_name_(event_name),
      begin_ticks_(begin_ticks) {}

TTCJournal::PendingAsyncEntry::~PendingAsyncEntry() {
  if (!ended_) {
    EndEntry();
  }
}

void TTCJournal::PendingAsyncEntry::EndEntry(
    std::vector<TTCJournalDetail> details) {
  if (ended_) {
    return;
  }
  ended_ = true;
  if (!journal_) {
    return;
  }
  const int64_t duration_ms = std::max<int64_t>(
      0, (base::TimeTicks::Now() - begin_ticks_).InMilliseconds());
  details.push_back({"duration_ms", base::NumberToString(duration_ms)});
  journal_->AppendEntry(component_, TTCJournalEntryType::kEnd,
                        base::Time::Now(), event_name_, std::move(details));
}

TTCJournal::TTCJournal(size_t max_entries)
    : max_entries_(std::max<size_t>(1, max_entries)) {}

TTCJournal::~TTCJournal() = default;

void TTCJournal::Log(TTCJournalComponent component,
                     std::string_view event_name,
                     std::vector<TTCJournalDetail> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AppendEntry(component, TTCJournalEntryType::kInstant, base::Time::Now(),
              event_name, std::move(details));
}

void TTCJournal::LogError(TTCJournalComponent component,
                          std::string_view event_name,
                          std::vector<TTCJournalDetail> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AppendEntry(component, TTCJournalEntryType::kError, base::Time::Now(),
              event_name, std::move(details));
}

std::unique_ptr<TTCJournal::PendingAsyncEntry> TTCJournal::BeginAsyncEvent(
    TTCJournalComponent component,
    std::string_view event_name,
    std::vector<TTCJournalDetail> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AppendEntry(component, TTCJournalEntryType::kBegin, base::Time::Now(),
              event_name, std::move(details));
  return std::make_unique<PendingAsyncEntry>(
      async_entry_weak_factory_.GetWeakPtr(), component, event_name,
      base::TimeTicks::Now());
}

void TTCJournal::HandleServerJournalEvent(
    const ttc::ServerJournalEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TTCJournalDetailsBuilder details_builder;
  for (const ttc::ServerJournalEvent::Details& detail : event.details) {
    details_builder.Add(detail.key, detail.value);
  }
  const base::Time timestamp = event.timestamp + server_clock_delta_;

  if (event.type != ttc::ServerJournalEvent::Type::kClockSync &&
      !received_clock_sync_) {
    AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kInstant,
                base::Time::Now(), "ServerJournalEventBeforeClockSync", {});
  }

  switch (event.type) {
    case ttc::ServerJournalEvent::Type::kInstant:
      AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kInstant,
                  timestamp, event.name, std::move(details_builder).Build());
      break;
    case ttc::ServerJournalEvent::Type::kAsyncBegin: {
      auto existing_it =
          pending_server_async_events_.find(event.async_event_id);
      if (existing_it != pending_server_async_events_.end()) {
        AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kEnd,
                    timestamp, existing_it->second.event_name, {});
        pending_server_async_events_.erase(existing_it);
      }
      pending_server_async_events_[event.async_event_id] = {
          .event_name = event.name,
          .begin_timestamp = timestamp,
      };
      AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kBegin,
                  timestamp, event.name, std::move(details_builder).Build());
      break;
    }
    case ttc::ServerJournalEvent::Type::kAsyncEnd: {
      auto it = pending_server_async_events_.find(event.async_event_id);
      if (it == pending_server_async_events_.end()) {
        details_builder.Add("event_name", event.name)
            .Add("async_event_id", event.async_event_id);
        AppendEntry(TTCJournalComponent::kBackend,
                    TTCJournalEntryType::kInstant, timestamp,
                    "UnmatchedAsyncEnd", std::move(details_builder).Build());
        break;
      }
      const int64_t duration_ms = std::max<int64_t>(
          0, (timestamp - it->second.begin_timestamp).InMilliseconds());
      details_builder.Add("duration_ms", duration_ms);
      const std::string ended_name =
          event.name.empty() ? it->second.event_name : event.name;
      pending_server_async_events_.erase(it);
      AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kEnd,
                  timestamp, ended_name, std::move(details_builder).Build());
      break;
    }
    case ttc::ServerJournalEvent::Type::kClockSync: {
      received_clock_sync_ = true;
      if (event.sync_timestamp <= base::Time::UnixEpoch()) {
        AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kError,
                    base::Time::Now(), "Bad ClockSync Message",
                    TTCJournalDetailsBuilder()
                        .Add("sync_timestamp", event.sync_timestamp)
                        .Build());
        break;
      }
      const base::Time client_receive_time = base::Time::Now();
      const base::Time client_midpoint =
          event.sync_timestamp +
          (client_receive_time - event.sync_timestamp) / 2;
      server_clock_delta_ = client_midpoint - event.timestamp;
      details_builder.Add("clock_delta", server_clock_delta_);
      AppendEntry(TTCJournalComponent::kBackend, TTCJournalEntryType::kInstant,
                  client_midpoint,
                  event.name.empty() ? "ClockSync" : event.name,
                  std::move(details_builder).Build());
      break;
    }
    case ttc::ServerJournalEvent::Type::kUnspecified:
      break;
  }
}

void TTCJournal::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.clear();
  received_clock_sync_ = false;
  server_clock_delta_ = base::TimeDelta();
  pending_server_async_events_.clear();
  async_entry_weak_factory_.InvalidateWeakPtrs();
  cleared_callbacks_.Notify();
}

base::CallbackListSubscription TTCJournal::AddEntryAddedListener(
    EntryAddedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entry_added_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription TTCJournal::AddClearedListener(
    ClearedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cleared_callbacks_.Add(std::move(callback));
}

base::WeakPtr<TTCJournal> TTCJournal::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void TTCJournal::AppendEntry(TTCJournalComponent component,
                             TTCJournalEntryType type,
                             base::Time timestamp,
                             std::string_view event_name,
                             std::vector<TTCJournalDetail> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (entries_.size() >= max_entries_) {
    entries_.pop_front();
  }

  TTCJournalEntry entry;
  entry.id = next_entry_id_++;
  entry.timestamp = timestamp;
  entry.component = component;
  entry.type = type;
  entry.event_name = std::string(event_name);
  entry.details = std::move(details);

  entries_.push_back(std::move(entry));
  entry_added_callbacks_.Notify(entries_.back());
}
