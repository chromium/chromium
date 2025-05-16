// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/trace_net_log_observer.h"

#include <stdio.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "net/base/tracing.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event.h"

namespace net {

namespace {

// TraceLog category for NetLog events.
constexpr const char kNetLogTracingCategory[] = "netlog";
constexpr const char kSensitiveNetLogTracingCategory[] =
    TRACE_DISABLED_BY_DEFAULT("netlog.sensitive");

class TracedValue : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit TracedValue(base::Value::Dict value) : value_(std::move(value)) {}
  ~TracedValue() override = default;

 private:
  void AppendAsTraceFormat(std::string* out) const override {
    if (!value_.empty()) {
      std::string tmp;
      base::JSONWriter::Write(value_, &tmp);
      *out += tmp;
    } else {
      *out += "{}";
    }
  }

 private:
  base::Value::Dict value_;
};

}  // namespace

TraceNetLogObserver::TraceNetLogObserver(Options options)
    : capture_mode_(options.capture_mode),
      use_sensitive_category_(options.use_sensitive_category) {}

TraceNetLogObserver::~TraceNetLogObserver() {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
}

void TraceNetLogObserver::OnAddEntry(const NetLogEntry& entry) {
  base::Value::Dict params = entry.params.Clone();
  // Add source's start time as a parameter. The net-log viewer requires it.
  params.Set("source_start_time",
             NetLog::TickCountToString(entry.source.start_time));
  const auto track = perfetto::Track(track_id_base_ + entry.source.id);

  // The tracing category must be a compile-time constant, hence the macro to
  // handle the sensitive vs non-sensitive categories.
#define CALL_TRACE_EVENT(TRACE_EVENT, ...)                       \
  do {                                                           \
    if (use_sensitive_category_) {                               \
      TRACE_EVENT(kSensitiveNetLogTracingCategory, __VA_ARGS__); \
    } else {                                                     \
      TRACE_EVENT(kNetLogTracingCategory, __VA_ARGS__);          \
    }                                                            \
  } while (false)

  switch (entry.phase) {
    case NetLogEventPhase::BEGIN:
      CALL_TRACE_EVENT(
          TRACE_EVENT_BEGIN,
          perfetto::StaticString(NetLogEventTypeToString(entry.type)), track,
          "source_type", NetLog::SourceTypeToString(entry.source.type),
          "params", std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::END:
      CALL_TRACE_EVENT(TRACE_EVENT_END, track, "params",
                       std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::NONE:
      CALL_TRACE_EVENT(
          TRACE_EVENT_INSTANT,
          perfetto::StaticString(NetLogEventTypeToString(entry.type)), track,
          "source_type", NetLog::SourceTypeToString(entry.source.type),
          "params", std::make_unique<TracedValue>(std::move(params)));
      break;
  }

#undef CALL_TRACE_EVENT
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

  net_log_to_watch_->AddObserver(this, capture_mode_);
}

void TraceNetLogObserver::OnTraceLogDisabled() {
  if (net_log())
    net_log()->RemoveObserver(this);
}

}  // namespace net
