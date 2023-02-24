// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log.h"

#include "base/synchronization/lock.h"
#include "base/values.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"

namespace net {

RecordingNetLogObserver::RecordingNetLogObserver()
    : RecordingNetLogObserver(NetLogCaptureMode::kIncludeSensitive) {}

RecordingNetLogObserver::RecordingNetLogObserver(NetLogCaptureMode capture_mode)
    : RecordingNetLogObserver(NetLog::Get(), capture_mode) {}

RecordingNetLogObserver::RecordingNetLogObserver(NetLog* net_log,
                                                 NetLogCaptureMode capture_mode)
    : net_log_(net_log) {
  net_log_->AddObserver(this, capture_mode);
}

RecordingNetLogObserver::~RecordingNetLogObserver() {
  net_log_->RemoveObserver(this);
}

std::vector<NetLogEntry> RecordingNetLogObserver::GetEntries() const {
  base::AutoLock lock(lock_);
  std::vector<NetLogEntry> result;
  for (const auto& entry : entry_list_)
    result.push_back(entry.Clone());
  return result;
}

std::vector<NetLogEntry> RecordingNetLogObserver::GetEntriesForSource(
    NetLogSource source) const {
  base::AutoLock lock(lock_);
  std::vector<NetLogEntry> result;
  for (const auto& entry : entry_list_) {
    if (entry.source.id == source.id)
      result.push_back(entry.Clone());
  }
  return result;
}

std::vector<NetLogEntry> RecordingNetLogObserver::GetEntriesWithType(
    NetLogEventType type) const {
  base::AutoLock lock(lock_);
  std::vector<NetLogEntry> result;
  for (const auto& entry : entry_list_) {
    if (entry.type == type)
      result.push_back(entry.Clone());
  }
  return result;
}

std::vector<NetLogEntry> RecordingNetLogObserver::GetEntriesForSourceWithType(
    NetLogSource source,
    NetLogEventType type,
    NetLogEventPhase phase) const {
  base::AutoLock lock(lock_);
  std::vector<NetLogEntry> result;
  for (const auto& entry : entry_list_) {
    if (entry.source.id == source.id && entry.type == type &&
        entry.phase == phase) {
      result.push_back(entry.Clone());
    }
  }
  return result;
}

size_t RecordingNetLogObserver::GetSize() const {
  base::AutoLock lock(lock_);
  return entry_list_.size();
}

void RecordingNetLogObserver::Clear() {
  base::AutoLock lock(lock_);
  entry_list_.clear();
}

void RecordingNetLogObserver::OnAddEntry(const NetLogEntry& entry) {
  base::Value::Dict params = entry.params.Clone();
  base::RepeatingClosure add_entry_callback;
  {
    // Only need to acquire the lock when accessing class variables.
    base::AutoLock lock(lock_);
    entry_list_.emplace_back(entry.type, entry.source, entry.phase, entry.time,
                             std::move(params));
    add_entry_callback = add_entry_callback_;
  }
  if (!add_entry_callback.is_null())
    add_entry_callback.Run();
}

void RecordingNetLogObserver::SetObserverCaptureMode(
    NetLogCaptureMode capture_mode) {
  net_log_->RemoveObserver(this);
  net_log_->AddObserver(this, capture_mode);
}

void RecordingNetLogObserver::SetThreadsafeAddEntryCallback(
    base::RepeatingClosure add_entry_callback) {
  base::AutoLock lock(lock_);
  add_entry_callback_ = add_entry_callback;
}

}  // namespace net
