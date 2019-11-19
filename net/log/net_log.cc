// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/log/net_log_values.h"

namespace net {

NetLog::ThreadSafeObserver::ThreadSafeObserver()
    : capture_mode_(NetLogCaptureMode::kDefault), net_log_(nullptr) {}

NetLog::ThreadSafeObserver::~ThreadSafeObserver() {
  // Make sure we aren't watching a NetLog on destruction.  Because the NetLog
  // may pass events to each observer on multiple threads, we cannot safely
  // stop watching a NetLog automatically from a parent class.
  DCHECK(!net_log_);
}

NetLogCaptureMode NetLog::ThreadSafeObserver::capture_mode() const {
  DCHECK(net_log_);
  return capture_mode_;
}

NetLog* NetLog::ThreadSafeObserver::net_log() const {
  return net_log_;
}

NetLog::NetLog() : last_id_(0), observer_capture_modes_(0) {}

NetLog::~NetLog() {
  MarkDead();
}

void NetLog::AddEntry(NetLogEventType type,
                      const NetLogSource& source,
                      NetLogEventPhase phase) {
  AddEntry(type, source, phase, [] { return base::Value(); });
}

void NetLog::AddGlobalEntry(NetLogEventType type) {
  AddEntry(type, NetLogSource(NetLogSourceType::NONE, NextID()),
           NetLogEventPhase::NONE);
}

void NetLog::AddGlobalEntryWithStringParams(NetLogEventType type,
                                            base::StringPiece name,
                                            base::StringPiece value) {
  AddGlobalEntry(type, [&] { return NetLogParamsWithString(name, value); });
}

uint32_t NetLog::NextID() {
  return base::subtle::NoBarrier_AtomicIncrement(&last_id_, 1);
}

void NetLog::AddObserver(NetLog::ThreadSafeObserver* observer,
                         NetLogCaptureMode capture_mode) {
  base::AutoLock lock(lock_);

  DCHECK(!observer->net_log_);
  DCHECK(!HasObserver(observer));
  DCHECK_LT(observers_.size(), 20u);  // Performance sanity check.

  observers_.push_back(observer);

  observer->net_log_ = this;
  observer->capture_mode_ = capture_mode;
  UpdateObserverCaptureModes();
}

void NetLog::RemoveObserver(NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(this, observer->net_log_);

  auto it = std::find(observers_.begin(), observers_.end(), observer);
  DCHECK(it != observers_.end());
  observers_.erase(it);

  observer->net_log_ = nullptr;
  observer->capture_mode_ = NetLogCaptureMode::kDefault;
  UpdateObserverCaptureModes();
}

void NetLog::UpdateObserverCaptureModes() {
  lock_.AssertAcquired();

  NetLogCaptureModeSet capture_mode_set = 0;
  for (const auto* observer : observers_)
    NetLogCaptureModeSetAdd(observer->capture_mode_, &capture_mode_set);

  base::subtle::NoBarrier_Store(&observer_capture_modes_, capture_mode_set);
}

bool NetLog::HasObserver(ThreadSafeObserver* observer) {
  lock_.AssertAcquired();
  return base::Contains(observers_, observer);
}

// static
std::string NetLog::TickCountToString(const base::TimeTicks& time) {
  int64_t delta_time = time.since_origin().InMilliseconds();
  // TODO(https://crbug.com/915391): Use NetLogNumberValue().
  return base::NumberToString(delta_time);
}

// static
std::string NetLog::TimeToString(const base::Time& time) {
  // Convert the base::Time to its (approximate) equivalent in base::TimeTicks.
  base::TimeTicks time_ticks =
      base::TimeTicks::UnixEpoch() + (time - base::Time::UnixEpoch());
  return TickCountToString(time_ticks);
}

// static
const char* NetLog::EventTypeToString(NetLogEventType event) {
  switch (event) {
#define EVENT_TYPE(label)      \
  case NetLogEventType::label: \
    return #label;
#include "net/log/net_log_event_type_list.h"
#undef EVENT_TYPE
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
base::Value NetLog::GetEventTypesAsValue() {
  base::DictionaryValue dict;
  for (int i = 0; i < static_cast<int>(NetLogEventType::COUNT); ++i) {
    dict.SetInteger(EventTypeToString(static_cast<NetLogEventType>(i)), i);
  }
  return std::move(dict);
}

// static
const char* NetLog::SourceTypeToString(NetLogSourceType source) {
  switch (source) {
#define SOURCE_TYPE(label)      \
  case NetLogSourceType::label: \
    return #label;
#include "net/log/net_log_source_type_list.h"
#undef SOURCE_TYPE
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
base::Value NetLog::GetSourceTypesAsValue() {
  base::DictionaryValue dict;
  for (int i = 0; i < static_cast<int>(NetLogSourceType::COUNT); ++i) {
    dict.SetInteger(SourceTypeToString(static_cast<NetLogSourceType>(i)), i);
  }
  return std::move(dict);
}

// static
const char* NetLog::EventPhaseToString(NetLogEventPhase phase) {
  switch (phase) {
    case NetLogEventPhase::BEGIN:
      return "PHASE_BEGIN";
    case NetLogEventPhase::END:
      return "PHASE_END";
    case NetLogEventPhase::NONE:
      return "PHASE_NONE";
  }
  NOTREACHED();
  return nullptr;
}

void NetLog::AddEntryInternal(NetLogEventType type,
                              const NetLogSource& source,
                              NetLogEventPhase phase,
                              const GetParamsInterface* get_params) {
  NetLogCaptureModeSet observer_capture_modes = GetObserverCaptureModes();

  for (int i = 0; i <= static_cast<int>(NetLogCaptureMode::kLast); ++i) {
    NetLogCaptureMode capture_mode = static_cast<NetLogCaptureMode>(i);
    if (!NetLogCaptureModeSetContains(capture_mode, observer_capture_modes))
      continue;

    NetLogEntry entry(type, source, phase, base::TimeTicks::Now(),
                      get_params->GetParams(capture_mode));

    // Notify all of the log observers with |capture_mode|.
    base::AutoLock lock(lock_);
    for (auto* observer : observers_) {
      if (observer->capture_mode() == capture_mode)
        observer->OnAddEntry(entry);
    }
  }
}

void NetLog::AddEntryWithMaterializedParams(NetLogEventType type,
                                            const NetLogSource& source,
                                            NetLogEventPhase phase,
                                            base::Value&& params) {
  NetLogEntry entry(type, source, phase, base::TimeTicks::Now(),
                    std::move(params));

  // Notify all of the log observers with |capture_mode|.
  base::AutoLock lock(lock_);
  for (auto* observer : observers_) {
    observer->OnAddEntry(entry);
  }
}

}  // namespace net
