// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TEST_NET_LOG_H_
#define NET_LOG_TEST_NET_LOG_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

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
  NetLog* const net_log_;
  base::RepeatingClosure add_entry_callback_;

  DISALLOW_COPY_AND_ASSIGN(RecordingNetLogObserver);
};

// NetLog subclass that follows normal lifetime rules (has a public
// destructor.)
//
// This class is for testing only. Production code should use the singleton
// NetLog::Get().
class TestNetLog : public NetLog {
 public:
  TestNetLog();
  ~TestNetLog() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNetLog);
};

// NetLog subclass that attaches a single observer (this) to record NetLog
// events and their parameters into an in-memory buffer. The NetLog is observed
// at kSensitive level by default, however can be changed with
// SetObserverCaptureMode().
//
// This class is for testing only.
// RecordingNetLogObserver is preferred for new tests.
class RecordingTestNetLog : public TestNetLog {
 public:
  RecordingTestNetLog();
  ~RecordingTestNetLog() override;

  // These methods all delegate to the underlying RecordingNetLogObserver,
  // see the comments in that class for documentation.
  void SetObserverCaptureMode(NetLogCaptureMode capture_mode);
  std::vector<NetLogEntry> GetEntries() const;
  std::vector<NetLogEntry> GetEntriesForSource(NetLogSource source) const;
  std::vector<NetLogEntry> GetEntriesWithType(NetLogEventType type) const;
  std::vector<NetLogEntry> GetEntriesForSourceWithType(
      NetLogSource source,
      NetLogEventType type,
      NetLogEventPhase phase) const;
  size_t GetSize() const;
  void Clear();

  // Returns the NetLog observer responsible for recording the NetLog event
  // stream. For testing code that bypasses NetLogs and adds events directly to
  // an observer.
  NetLog::ThreadSafeObserver* GetObserver();

 private:
  RecordingNetLogObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(RecordingTestNetLog);
};

// Helper class that exposes a similar API as NetLogWithSource, but uses a
// RecordingTestNetLog rather than the more generic NetLog.
//
// A RecordingBoundTestNetLog can easily be converted to a NetLogWithSource
// using the bound() method.
class RecordingBoundTestNetLog {
 public:
  RecordingBoundTestNetLog();
  ~RecordingBoundTestNetLog();

  // The returned NetLogWithSource is only valid while |this| is alive.
  NetLogWithSource bound() const { return net_log_; }

  // These methods all delegate to the underlying RecordingNetLogObserver,
  // see the comments in that class for documentation.
  void SetObserverCaptureMode(NetLogCaptureMode capture_mode);
  std::vector<NetLogEntry> GetEntries() const;
  std::vector<NetLogEntry> GetEntriesForSource(NetLogSource source) const;
  std::vector<NetLogEntry> GetEntriesWithType(NetLogEventType type) const;
  std::vector<NetLogEntry> GetEntriesForSourceWithType(
      NetLogSource source,
      NetLogEventType type,
      NetLogEventPhase phase) const;
  size_t GetSize() const;
  void Clear();

 private:
  RecordingTestNetLog test_net_log_;
  const NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(RecordingBoundTestNetLog);
};

}  // namespace net

#endif  // NET_LOG_TEST_NET_LOG_H_
