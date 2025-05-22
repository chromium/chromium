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
#include "base/strings/stringprintf.h"
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

// Inspired by http://crbug.com/418158806#comment2. This is more efficient than
// using TrackEvent::SetTrackDescriptor() because that would require us to keep
// extra state to ensure we only call it once.
class SourceTrack final : public perfetto::Track {
 public:
  SourceTrack(uint64_t uuid,
              perfetto::Track parent_track,
              perfetto::StaticString source_type_string)
      : Track(uuid, parent_track), source_type_string_(source_type_string) {}

  void Serialize(
      perfetto::protos::pbzero::TrackDescriptor* track_descriptor) const {
    const auto bytes = Serialize().SerializeAsString();
    track_descriptor->AppendRawProtoBytes(bytes.data(), bytes.size());
  }
  perfetto::protos::gen::TrackDescriptor Serialize() const {
    auto track_descriptor = Track::Serialize();
    // We add a reasonably unique random string at the end of the track name
    // to prevent the Perfetto UI from incorrectly merging identically-named
    // tracks, which would be confusing in our case (e.g. separate independent
    // URL requests being incorrectly stacked on top of each other). See
    // https://crbug.com/417420482.
    //
    // Note this means the name is not really static, but it's fine to treat it
    // as such as the dynamic part obviously doesn't carry any sensitive
    // information.
    track_descriptor.set_static_name(base::StringPrintf(
        "%s %04x", source_type_string_.value, uuid % 0xFFFF));
    return track_descriptor;
  }

 private:
  perfetto::StaticString source_type_string_;
};

}  // namespace

TraceNetLogObserver::TraceNetLogObserver(Options options)
    : capture_mode_(options.capture_mode),
      use_sensitive_category_(options.use_sensitive_category),
      root_track_name_(options.root_track_name) {}

TraceNetLogObserver::~TraceNetLogObserver() {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
}

perfetto::Track TraceNetLogObserver::MaybeSetUpAndGetRootTrack() {
  // -1 to prevent conflicts with source tracks (which use positive offsets).
  perfetto::Track root_track(track_id_base_ - 1);
  std::call_once(root_track_set_up_, [&] {
    auto root_track_descriptor = root_track.Serialize();
    root_track_descriptor.set_static_name(root_track_name_.value);
    root_track_descriptor.set_child_ordering(
        perfetto::protos::gen::TrackDescriptor::CHRONOLOGICAL);
    base::TrackEvent::SetTrackDescriptor(root_track, root_track_descriptor);
  });
  return root_track;
}

void TraceNetLogObserver::OnAddEntry(const NetLogEntry& entry) {
  base::Value::Dict params = entry.params.Clone();
  // Add source's start time as a parameter. The net-log viewer requires it.
  params.Set("source_start_time",
             NetLog::TickCountToString(entry.source.start_time));
  const perfetto::StaticString source_type_string(
      NetLog::SourceTypeToString(entry.source.type));
  const SourceTrack track(track_id_base_ + entry.source.id,
                          MaybeSetUpAndGetRootTrack(), source_type_string);

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
          "source_type", source_type_string, "params",
          std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::END:
      CALL_TRACE_EVENT(TRACE_EVENT_END, track, "params",
                       std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::NONE:
      CALL_TRACE_EVENT(
          TRACE_EVENT_INSTANT,
          perfetto::StaticString(NetLogEventTypeToString(entry.type)), track,
          "source_type", source_type_string, "params",
          std::make_unique<TracedValue>(std::move(params)));
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
