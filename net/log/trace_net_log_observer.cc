// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/trace_net_log_observer.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"

namespace net {

namespace {

// TraceLog category for NetLog events.
constexpr const char kNetLogTracingCategory[] = "netlog";

class TracedValue : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit TracedValue(base::Value value) : value_(std::move(value)) {}

 private:
  ~TracedValue() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    if (!value_.is_none()) {
      std::string tmp;
      base::JSONWriter::Write(value_, &tmp);
      *out += tmp;
    } else {
      *out += "\"\"";
    }
  }

 private:
  base::Value value_;
};

}  // namespace

TraceNetLogObserver::TraceNetLogObserver() : net_log_to_watch_(nullptr) {}

TraceNetLogObserver::~TraceNetLogObserver() {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
}

void TraceNetLogObserver::OnAddEntry(const NetLogEntry& entry) {
  base::Value params = entry.params.Clone();
  switch (entry.phase) {
    case NetLogEventPhase::BEGIN:
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type),
          entry.source.id, "source_type",
          NetLog::SourceTypeToString(entry.source.type), "params",
          std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(std::move(params))));
      break;
    case NetLogEventPhase::END:
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type),
          entry.source.id, "source_type",
          NetLog::SourceTypeToString(entry.source.type), "params",
          std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(std::move(params))));
      break;
    case NetLogEventPhase::NONE:
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type),
          entry.source.id, "source_type",
          NetLog::SourceTypeToString(entry.source.type), "params",
          std::unique_ptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(std::move(params))));
      break;
  }
}

void TraceNetLogObserver::WatchForTraceStart(NetLog* netlog) {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
  net_log_to_watch_ = netlog;
  // Tracing can start before the observer is even created, for instance for
  // startup tracing.
  if (base::trace_event::TraceLog::GetInstance()->IsEnabled())
    OnTraceLogEnabled();
  base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
      weak_factory_.GetWeakPtr());
}

void TraceNetLogObserver::StopWatchForTraceStart() {
  // Should only stop if is currently watching.
  DCHECK(net_log_to_watch_);
  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      this);
  // net_log() != nullptr iff NetLog::AddObserver() has been called.
  // This implies that if the netlog category wasn't enabled, then
  // NetLog::RemoveObserver() will not get called, and there won't be
  // a crash in NetLog::RemoveObserver().
  if (net_log())
    net_log()->RemoveObserver(this);
  net_log_to_watch_ = nullptr;
}

void TraceNetLogObserver::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kNetLogTracingCategory, &enabled);
  if (!enabled)
    return;

  net_log_to_watch_->AddObserver(this, NetLogCaptureMode::kDefault);
}

void TraceNetLogObserver::OnTraceLogDisabled() {
  if (net_log())
    net_log()->RemoveObserver(this);
}

}  // namespace net
