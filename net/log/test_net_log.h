// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TEST_NET_LOG_H_
#define NET_LOG_TEST_NET_LOG_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"

namespace net {

struct NetLogSource;

// NetLog subclass that attaches a single observer (this) to record NetLog
// events and their parameters into an in-memory buffer. The NetLog is observed
// at kSensitive level by default, however can be changed with
// SetObserverCaptureMode().
//
// This class is for testing only.
class TestNetLog : public NetLog, public NetLog::ThreadSafeObserver {
 public:
  TestNetLog();
  ~TestNetLog() override;

  void SetObserverCaptureMode(NetLogCaptureMode capture_mode);

  // ThreadSafeObserver implementation:
  void OnAddEntry(const NetLogEntry& entry) override;

  // Returns the list of all observed NetLog entries.
  std::vector<NetLogEntry> GetEntries() const;

  // Returns all entries in the log from the specified Source.
  std::vector<NetLogEntry> GetEntriesForSource(NetLogSource source) const;

  // Returns all captured entries with the specified type.
  std::vector<NetLogEntry> GetEntriesWithType(NetLogEventType type) const;

  // Returns the number of entries in the log.
  size_t GetSize() const;

  // Clears the captured entry list.
  void Clear();

  // Returns the NetLog observer responsible for recording the NetLog event
  // stream. For testing code that bypasses NetLogs and adds events directly to
  // an observer.
  NetLog::ThreadSafeObserver* GetObserver();

 private:
  mutable base::Lock lock_;
  std::vector<NetLogEntry> entry_list_;

  DISALLOW_COPY_AND_ASSIGN(TestNetLog);
};

// Helper class that exposes a similar API as NetLogWithSource, but uses a
// TestNetLog rather than the more generic NetLog.
//
// A BoundTestNetLog can easily be converted to a NetLogWithSource using the
// bound() method.
class BoundTestNetLog {
 public:
  BoundTestNetLog();
  ~BoundTestNetLog();

  // The returned NetLogWithSource is only valid while |this| is alive.
  NetLogWithSource bound() const { return net_log_; }

  // Returns all captured entries.
  std::vector<NetLogEntry> GetEntries() const;

  // Returns all captured entries for the specified Source.
  std::vector<NetLogEntry> GetEntriesForSource(NetLogSource source) const;

  // Returns all captured entries with the specified type.
  std::vector<NetLogEntry> GetEntriesWithType(NetLogEventType type) const;

  // Returns number of entries in the log.
  size_t GetSize() const;

  void Clear();

  // Sets the observer capture mode of the underlying TestNetLog.
  void SetObserverCaptureMode(NetLogCaptureMode capture_mode);

 private:
  TestNetLog test_net_log_;
  const NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(BoundTestNetLog);
};

}  // namespace net

#endif  // NET_LOG_TEST_NET_LOG_H_
