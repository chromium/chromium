// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_AGGREGATED_JOURNAL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_AGGREGATED_JOURNAL_H_

#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/containers/ring_buffer.h"
#import "base/memory/safe_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "base/types/pass_key.h"

class GURL;

struct TaskId {
  int32_t id;
};

// Mirrored from chrome/common/actor.mojom.
enum class JournalEntryType {
  kBegin,
  kEnd,
  kInstant,
};

// Mirrored from chrome/common/actor.mojom.
struct JournalDetails {
  std::string key;
  std::string value;
};

// Represents a journal entry. This should only be used for logging and
// debugging. It should not be used to make logic decisions since a
// compromised renderer could lie about events (such as mismatched
// or missing begin and end events).
//
// Mirrored from chrome/common/actor.mojom.
struct JournalEntry {
  JournalEntry();
  JournalEntry(const JournalEntry&);
  JournalEntry& operator=(const JournalEntry&);
  ~JournalEntry();

  JournalEntryType type;
  TaskId task_id;
  base::Time timestamp;
  std::string event;
  uint64_t track_uuid;
  std::vector<JournalDetails> details;
};

// A class that holds logs emitted when actuating Chrome on iOS.
// This class must be used on a single sequence (typically the UI thread).
//
// Mirrored from chrome/browser/actor/aggregated_journal.h.
class AggregatedJournal {
 public:
  // A pending async journal entry.
  class PendingAsyncEntry {
   public:
    PendingAsyncEntry(base::PassKey<AggregatedJournal>,
                      base::SafeRef<AggregatedJournal> journal,
                      TaskId task_id,
                      std::string_view event_name,
                      uint64_t track_uuid);
    ~PendingAsyncEntry();

    // End a pending entry with additional details.
    void EndEntry(std::vector<JournalDetails> details);

    AggregatedJournal& GetJournal();
    TaskId GetTaskId();

    const std::string& event_name() const { return event_name_; }
    base::TimeTicks begin_time() const { return begin_time_; }

   private:
    bool terminated_ = false;
    base::SafeRef<AggregatedJournal> journal_;
    TaskId task_id_;
    std::string event_name_;
    base::TimeTicks begin_time_;
    uint64_t track_uuid_;
  };

  // Mirrored from chrome/browser/actor/aggregated_journal.h.
  struct Entry {
    std::string url;
    std::unique_ptr<JournalEntry> data;

    Entry(const std::string& location, std::unique_ptr<JournalEntry> data);
    ~Entry();
  };

  using EntryBuffer = base::RingBuffer<std::unique_ptr<Entry>, 20>;

  AggregatedJournal();
  ~AggregatedJournal();

  AggregatedJournal(const AggregatedJournal&) = delete;
  AggregatedJournal& operator=(const AggregatedJournal&) = delete;

  uint64_t AllocateDynamicTrackUUID();

  // Create an async entry. This will log a Begin event immediately.
  // When the returned object is destroyed, it logs an End event.
  std::unique_ptr<PendingAsyncEntry> CreatePendingAsyncEntry(
      const GURL& url,
      TaskId task_id,
      uint64_t track_uuid,
      std::string_view event_name,
      std::vector<JournalDetails> details);

  // Log an instant event.
  void Log(const GURL& url,
           TaskId task_id,
           std::string_view event_name,
           std::vector<JournalDetails> details);

  // Log an instant event with a specific track.
  void Log(const GURL& url,
           TaskId task_id,
           uint64_t track_uuid,
           std::string_view event_name,
           std::vector<JournalDetails> details);

  std::vector<JournalEntry> GetLogs() const;

  // Returns all logs formatted as a JSON string.
  std::string GetLogsAsJson() const;

  void Clear();

  base::SafeRef<AggregatedJournal> GetSafeRef();
  base::WeakPtr<AggregatedJournal> GetWeakPtr();
  void AddEndEvent(base::PassKey<AggregatedJournal>,
                   TaskId task_id,
                   const std::string& event_name,
                   uint64_t track_uuid,
                   std::vector<JournalDetails> details);

 private:
  void AddEntry(std::unique_ptr<Entry> entry);

  EntryBuffer entries_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AggregatedJournal> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_AGGREGATED_JOURNAL_H_
