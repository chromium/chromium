// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log.h"

#include "base/macros.h"
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
  base::Value params = entry.params.Clone();
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

TestNetLog::TestNetLog() : NetLog(base::PassKey<TestNetLog>()) {}
TestNetLog::~TestNetLog() = default;

RecordingTestNetLog::RecordingTestNetLog()
    : observer_(this, NetLogCaptureMode::kIncludeSensitive) {}
RecordingTestNetLog::~RecordingTestNetLog() = default;

std::vector<NetLogEntry> RecordingTestNetLog::GetEntries() const {
  return observer_.GetEntries();
}

std::vector<NetLogEntry> RecordingTestNetLog::GetEntriesForSource(
    NetLogSource source) const {
  return observer_.GetEntriesForSource(source);
}

std::vector<NetLogEntry> RecordingTestNetLog::GetEntriesWithType(
    NetLogEventType type) const {
  return observer_.GetEntriesWithType(type);
}

std::vector<NetLogEntry> RecordingTestNetLog::GetEntriesForSourceWithType(
    NetLogSource source,
    NetLogEventType type,
    NetLogEventPhase phase) const {
  return observer_.GetEntriesForSourceWithType(source, type, phase);
}

size_t RecordingTestNetLog::GetSize() const {
  return observer_.GetSize();
}

void RecordingTestNetLog::Clear() {
  return observer_.Clear();
}

NetLog::ThreadSafeObserver* RecordingTestNetLog::GetObserver() {
  return &observer_;
}

void RecordingTestNetLog::SetObserverCaptureMode(
    NetLogCaptureMode capture_mode) {
  observer_.SetObserverCaptureMode(capture_mode);
}

RecordingBoundTestNetLog::RecordingBoundTestNetLog()
    : net_log_(NetLogWithSource::Make(&test_net_log_, NetLogSourceType::NONE)) {
}

RecordingBoundTestNetLog::~RecordingBoundTestNetLog() = default;

std::vector<NetLogEntry> RecordingBoundTestNetLog::GetEntries() const {
  return test_net_log_.GetEntries();
}

std::vector<NetLogEntry> RecordingBoundTestNetLog::GetEntriesForSource(
    NetLogSource source) const {
  return test_net_log_.GetEntriesForSource(source);
}

std::vector<NetLogEntry> RecordingBoundTestNetLog::GetEntriesWithType(
    NetLogEventType type) const {
  return test_net_log_.GetEntriesWithType(type);
}

std::vector<NetLogEntry> RecordingBoundTestNetLog::GetEntriesForSourceWithType(
    NetLogSource source,
    NetLogEventType type,
    NetLogEventPhase phase) const {
  return test_net_log_.GetEntriesForSourceWithType(source, type, phase);
}

size_t RecordingBoundTestNetLog::GetSize() const {
  return test_net_log_.GetSize();
}

void RecordingBoundTestNetLog::Clear() {
  test_net_log_.Clear();
}

void RecordingBoundTestNetLog::SetObserverCaptureMode(
    NetLogCaptureMode capture_mode) {
  test_net_log_.SetObserverCaptureMode(capture_mode);
}

}  // namespace net
