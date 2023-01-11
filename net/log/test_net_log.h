// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TEST_NET_LOG_H_
#define NET_LOG_TEST_NET_LOG_H_

#include <stddef.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"

namespace net {

struct NetLogSource;

// NetLog observer that record NetLogs events and their parameters into an
// in-memory buffer.
//
// This class is for testing only.
class RecordingNetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  // Observe the global singleton netlog with kIncludeSensitive capture mode.
  RecordingNetLogObserver();

  // Observe the global singleton netlog with |capture_mode|.
  explicit RecordingNetLogObserver(NetLogCaptureMode capture_mode);

  // Observe the specified |net_log| object with |capture_mode|.
  RecordingNetLogObserver(NetLog* net_log, NetLogCaptureMode capture_mode);

  RecordingNetLogObserver(const RecordingNetLogObserver&) = delete;
  RecordingNetLogObserver& operator=(const RecordingNetLogObserver&) = delete;

  ~RecordingNetLogObserver() override;

  // Change the |capture_mode|.
  void SetObserverCaptureMode(NetLogCaptureMode capture_mode);

  // |add_entry_callback| may be called on any thread.
  void SetThreadsafeAddEntryCallback(base::RepeatingClosure add_entry_callback);

  // ThreadSafeObserver implementation:
  void OnAddEntry(const NetLogEntry& entry) override;

  // Returns the list of all observed NetLog entries.
  std::vector<NetLogEntry> GetEntries() const;

  // Returns all entries in the log from the specified Source.
  std::vector<NetLogEntry> GetEntriesForSource(NetLogSource source) const;

  // Returns all captured entries with the specified type.
  std::vector<NetLogEntry> GetEntriesWithType(NetLogEventType type) const;

  // Returns all captured entries with the specified values.
  std::vector<NetLogEntry> GetEntriesForSourceWithType(
      NetLogSource source,
      NetLogEventType type,
      NetLogEventPhase phase) const;

  // Returns the number of entries in the log.
  size_t GetSize() const;

  // Clears the captured entry list.
  void Clear();

 private:
  mutable base::Lock lock_;
  std::vector<NetLogEntry> entry_list_;
  const raw_ptr<NetLog> net_log_;
  base::RepeatingClosure add_entry_callback_;
};

}  // namespace net

#endif  // NET_LOG_TEST_NET_LOG_H_
