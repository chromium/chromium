// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_JOURNAL_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_JOURNAL_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "components/ttc/app/public/server_journal_event.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

// Identifies which TTC model subsystem produced a journal entry.
enum class TTCJournalComponent {
  kSession,
  kConversation,
  kBackend,
  kAudio,
  kActuation,
};

// Returns the short display label for `component` (e.g. "Session", "Conv",
// "Backend", "Audio", "Actuation").
std::string_view TTCJournalComponentToString(TTCJournalComponent component);

// Entry type recorded in `TTCJournal`.
enum class TTCJournalEntryType {
  kInstant,
  kBegin,
  kEnd,
  kError,
};

// Key-value metadata pair attached to a `TTCJournalEntry`.
struct TTCJournalDetail {
  std::string key;
  std::string value;

  bool operator==(const TTCJournalDetail&) const = default;
};

// Builder for constructing `std::vector<TTCJournalDetail>` ergonomically.
class TTCJournalDetailsBuilder {
 public:
  TTCJournalDetailsBuilder();
  TTCJournalDetailsBuilder(TTCJournalDetailsBuilder&&);
  TTCJournalDetailsBuilder& operator=(TTCJournalDetailsBuilder&&);
  ~TTCJournalDetailsBuilder();

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  TTCJournalDetailsBuilder Add(std::string_view key,
                               const ValueType& value) && {
    details_.push_back({std::string(key), base::ToString(value)});
    return std::move(*this);
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  TTCJournalDetailsBuilder& Add(std::string_view key,
                                const ValueType& value) & {
    details_.push_back({std::string(key), base::ToString(value)});
    return *this;
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  TTCJournalDetailsBuilder AddError(const ValueType& value) && {
    details_.push_back({"error", base::ToString(value)});
    return std::move(*this);
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  TTCJournalDetailsBuilder& AddError(const ValueType& value) & {
    details_.push_back({"error", base::ToString(value)});
    return *this;
  }

  std::vector<TTCJournalDetail> Build() && { return std::move(details_); }

 private:
  std::vector<TTCJournalDetail> details_;
};

// A single structured entry in `TTCJournal`.
struct TTCJournalEntry {
  TTCJournalEntry();
  TTCJournalEntry(const TTCJournalEntry&);
  TTCJournalEntry& operator=(const TTCJournalEntry&);
  TTCJournalEntry(TTCJournalEntry&&);
  TTCJournalEntry& operator=(TTCJournalEntry&&);
  ~TTCJournalEntry();

  uint64_t id = 0;
  base::Time timestamp;
  TTCJournalComponent component = TTCJournalComponent::kSession;
  TTCJournalEntryType type = TTCJournalEntryType::kInstant;
  std::string event_name;
  std::vector<TTCJournalDetail> details;

  // Formats the entry as a single human-readable monospaced line:
  // "HH:MM:SS.mmm [Component] EventName (key=val, ...)"
  std::string FormatLine() const;
};

// Shared in-memory session journal for iOS TTC model components.
// Stores structured events in a bounded FIFO buffer and notifies UI listeners
// without writing to the system console.
class TTCJournal {
 public:
  static constexpr size_t kMaxEntries = 500;

  using EntryAddedCallback =
      base::RepeatingCallback<void(const TTCJournalEntry&)>;
  using ClearedCallback = base::RepeatingClosure;

  // RAII handle for an in-flight asynchronous operation. Logs a `kBegin` entry
  // on creation and a matching `kEnd` entry (including `duration_ms`) when
  // `EndEntry()` is called or when destroyed.
  class PendingAsyncEntry {
   public:
    PendingAsyncEntry(base::WeakPtr<TTCJournal> journal,
                      TTCJournalComponent component,
                      std::string_view event_name,
                      base::TimeTicks begin_ticks);
    ~PendingAsyncEntry();

    PendingAsyncEntry(const PendingAsyncEntry&) = delete;
    PendingAsyncEntry& operator=(const PendingAsyncEntry&) = delete;

    void EndEntry(std::vector<TTCJournalDetail> details = {});

   private:
    base::WeakPtr<TTCJournal> journal_;
    TTCJournalComponent component_;
    std::string event_name_;
    base::TimeTicks begin_ticks_;
    bool ended_ = false;
  };

  explicit TTCJournal(size_t max_entries = kMaxEntries);
  ~TTCJournal();

  TTCJournal(const TTCJournal&) = delete;
  TTCJournal& operator=(const TTCJournal&) = delete;

  // Logs an instant event under `component`.
  void Log(TTCJournalComponent component,
           std::string_view event_name,
           std::vector<TTCJournalDetail> details = {});

  // Logs an error event under `component`.
  void LogError(TTCJournalComponent component,
                std::string_view event_name,
                std::vector<TTCJournalDetail> details = {});

  // Begins a scoped async event under `component`.
  [[nodiscard]] std::unique_ptr<PendingAsyncEntry> BeginAsyncEvent(
      TTCJournalComponent component,
      std::string_view event_name,
      std::vector<TTCJournalDetail> details = {});

  // Ingests a `ttc::ServerJournalEvent` from the backend under `kBackend`.
  void HandleServerJournalEvent(const ttc::ServerJournalEvent& event);

  // Clears all buffered entries and resets pending server async state.
  void Clear();

  // Returns the current buffered entries in chronological order.
  const std::deque<TTCJournalEntry>& entries() const { return entries_; }

  // Subscriptions for UI hydration and live updates:
  base::CallbackListSubscription AddEntryAddedListener(
      EntryAddedCallback callback);
  base::CallbackListSubscription AddClearedListener(ClearedCallback callback);

  base::TimeDelta server_clock_delta() const { return server_clock_delta_; }

  base::WeakPtr<TTCJournal> GetWeakPtr();

 private:
  struct PendingServerAsyncEvent {
    std::string event_name;
    base::Time begin_timestamp;
  };

  void AppendEntry(TTCJournalComponent component,
                   TTCJournalEntryType type,
                   base::Time timestamp,
                   std::string_view event_name,
                   std::vector<TTCJournalDetail> details);

  SEQUENCE_CHECKER(sequence_checker_);

  const size_t max_entries_;
  uint64_t next_entry_id_ = 1;
  std::deque<TTCJournalEntry> entries_;

  bool received_clock_sync_ = false;
  base::TimeDelta server_clock_delta_;
  absl::flat_hash_map<int32_t, PendingServerAsyncEvent>
      pending_server_async_events_;

  base::RepeatingCallbackList<void(const TTCJournalEntry&)>
      entry_added_callbacks_;
  base::RepeatingClosureList cleared_callbacks_;

  base::WeakPtrFactory<TTCJournal> async_entry_weak_factory_{this};
  base::WeakPtrFactory<TTCJournal> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_JOURNAL_H_
