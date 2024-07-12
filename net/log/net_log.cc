// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/log/net_log_values.h"

namespace net {

NetLog::ThreadSafeObserver::ThreadSafeObserver() = default;

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

NetLog::ThreadSafeCaptureModeObserver::ThreadSafeCaptureModeObserver() =
    default;
NetLog::ThreadSafeCaptureModeObserver::~ThreadSafeCaptureModeObserver() =
    default;

NetLogCaptureModeSet
NetLog::ThreadSafeCaptureModeObserver::GetObserverCaptureModes() const {
  DCHECK(net_log_);
  return net_log_->GetObserverCaptureModes();
}

void NetLog::ThreadSafeCaptureModeObserver::
    AddEntryAtTimeWithMaterializedParams(NetLogEventType type,
                                         const NetLogSource& source,
                                         NetLogEventPhase phase,
                                         base::TimeTicks time,
                                         base::Value::Dict params) {
  DCHECK(net_log_);
  net_log_->AddEntryAtTimeWithMaterializedParams(type, source, phase, time,
                                                 std::move(params));
}

// static
NetLog* NetLog::Get() {
  static base::NoDestructor<NetLog> instance{base::PassKey<NetLog>()};
  return instance.get();
}

NetLog::NetLog(base::PassKey<NetLog>) {}
NetLog::NetLog(base::PassKey<NetLogWithSource>) {}

void NetLog::AddEntry(NetLogEventType type,
                      const NetLogSource& source,
                      NetLogEventPhase phase) {
  AddEntry(type, source, phase, [] { return base::Value::Dict(); });
}

void NetLog::AddGlobalEntry(NetLogEventType type) {
  AddEntry(type, NetLogSource(NetLogSourceType::NONE, NextID()),
           NetLogEventPhase::NONE);
}

void NetLog::AddGlobalEntryWithStringParams(NetLogEventType type,
                                            std::string_view name,
                                            std::string_view value) {
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

  auto it = base::ranges::find(observers_, observer);
  CHECK(it != observers_.end(), base::NotFatalUntil::M130);
  observers_.erase(it);

  observer->net_log_ = nullptr;
  observer->capture_mode_ = NetLogCaptureMode::kDefault;
  UpdateObserverCaptureModes();
}

void NetLog::AddCaptureModeObserver(
    NetLog::ThreadSafeCaptureModeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK(!observer->net_log_);
  DCHECK(!HasCaptureModeObserver(observer));
  DCHECK_LT(capture_mode_observers_.size(), 20u);  // Performance sanity check.

  observer->net_log_ = this;
  capture_mode_observers_.push_back(observer);
}

void NetLog::RemoveCaptureModeObserver(
    NetLog::ThreadSafeCaptureModeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK_EQ(this, observer->net_log_);
  DCHECK(HasCaptureModeObserver(observer));

  auto it = base::ranges::find(capture_mode_observers_, observer);
  CHECK(it != capture_mode_observers_.end(), base::NotFatalUntil::M130);
  capture_mode_observers_.erase(it);

  observer->net_log_ = nullptr;
}

void NetLog::UpdateObserverCaptureModes() {
  lock_.AssertAcquired();

  NetLogCaptureModeSet capture_mode_set = 0;
  for (const net::NetLog::ThreadSafeObserver* observer : observers_) {
    NetLogCaptureModeSetAdd(observer->capture_mode_, &capture_mode_set);
  }

  base::subtle::NoBarrier_Store(&observer_capture_modes_, capture_mode_set);

  // Notify any capture mode observers with the new |capture_mode_set|.
  for (net::NetLog::ThreadSafeCaptureModeObserver* capture_mode_observer :
       capture_mode_observers_) {
    capture_mode_observer->OnCaptureModeUpdated(capture_mode_set);
  }
}

bool NetLog::HasObserver(ThreadSafeObserver* observer) {
  lock_.AssertAcquired();
  return base::Contains(observers_, observer);
}

bool NetLog::HasCaptureModeObserver(ThreadSafeCaptureModeObserver* observer) {
  lock_.AssertAcquired();
  return base::Contains(capture_mode_observers_, observer);
}

// static
std::string NetLog::TickCountToString(const base::TimeTicks& time) {
  int64_t delta_time = time.since_origin().InMilliseconds();
  // TODO(crbug.com/40606676): Use NetLogNumberValue().
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
base::Value NetLog::GetEventTypesAsValue() {
  base::Value::Dict dict;
  for (int i = 0; i < static_cast<int>(NetLogEventType::COUNT); ++i) {
    dict.Set(NetLogEventTypeToString(static_cast<NetLogEventType>(i)), i);
  }
  return base::Value(std::move(dict));
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
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

// static
base::Value NetLog::GetSourceTypesAsValue() {
  base::Value::Dict dict;
  for (int i = 0; i < static_cast<int>(NetLogSourceType::COUNT); ++i) {
    dict.Set(SourceTypeToString(static_cast<NetLogSourceType>(i)), i);
  }
  return base::Value(std::move(dict));
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void NetLog::InitializeSourceIdPartition() {
  int32_t old_value = base::subtle::NoBarrier_AtomicExchange(
      &last_id_, std::numeric_limits<base::subtle::Atomic32>::min());
  DCHECK_EQ(old_value, 0) << " NetLog::InitializeSourceIdPartition() called "
                             "after NextID() or called multiple times";
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
    for (net::NetLog::ThreadSafeObserver* observer : observers_) {
      if (observer->capture_mode() == capture_mode)
        observer->OnAddEntry(entry);
    }
  }
}

void NetLog::AddEntryWithMaterializedParams(NetLogEventType type,
                                            const NetLogSource& source,
                                            NetLogEventPhase phase,
                                            base::Value::Dict params) {
  AddEntryAtTimeWithMaterializedParams(
      type, source, phase, base::TimeTicks::Now(), std::move(params));
}

void NetLog::AddEntryAtTimeWithMaterializedParams(NetLogEventType type,
                                                  const NetLogSource& source,
                                                  NetLogEventPhase phase,
                                                  base::TimeTicks time,
                                                  base::Value::Dict params) {
  NetLogEntry entry(type, source, phase, time, std::move(params));

  // Notify all of the log observers, regardless of capture mode.
  base::AutoLock lock(lock_);
  for (net::NetLog::ThreadSafeObserver* observer : observers_) {
    observer->OnAddEntry(entry);
  }
}

}  // namespace net
