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
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
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

perfetto::StaticString SourceTypeToStaticString(NetLogSourceType source_type) {
  return perfetto::StaticString(NetLog::SourceTypeToString(source_type));
}

}  // namespace

TraceNetLogObserver::TraceNetLogObserver(Options options)
    : capture_mode_(options.capture_mode),
      use_sensitive_category_(options.use_sensitive_category),
      verbose_(options.verbose),
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
  const perfetto::StaticString entry_type_string(
      NetLogEventTypeToString(entry.type));
  const auto source_type_string = SourceTypeToStaticString(entry.source.type);

  if (verbose_) {
    AddEntryVerbose(entry, entry_type_string, source_type_string,
                    std::move(params));
  } else {
    AddEntry(entry, entry_type_string, source_type_string, std::move(params));
  }
}

void TraceNetLogObserver::AddEntry(const NetLogEntry& entry,
                                   perfetto::StaticString entry_type_string,
                                   perfetto::StaticString source_type_string,
                                   base::Value::Dict params) {
  const perfetto::Track track(track_id_base_ + entry.source.id,
                              MaybeSetUpAndGetRootTrack());
  switch (entry.phase) {
    case NetLogEventPhase::BEGIN:
      TRACE_EVENT_BEGIN(kNetLogTracingCategory, entry_type_string, track,
                        "source_type", source_type_string, "params",
                        std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::END:
      TRACE_EVENT_END(kNetLogTracingCategory, track, "params",
                      std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::NONE:
      TRACE_EVENT_INSTANT(kNetLogTracingCategory, entry_type_string, track,
                          "source_type", source_type_string, "params",
                          std::make_unique<TracedValue>(std::move(params)));
      break;
  }
}

void TraceNetLogObserver::AddEntryVerbose(
    const NetLogEntry& entry,
    perfetto::StaticString entry_type_string,
    perfetto::StaticString source_type_string,
    base::Value::Dict params) {
  const auto get_source_track = [&](uint32_t source_id,
                                    perfetto::StaticString source_type_string) {
    return SourceTrack(track_id_base_ + entry.source.id,
                       MaybeSetUpAndGetRootTrack(), source_type_string);
  };
  const auto track = get_source_track(entry.source.id, source_type_string);

  // We use Perfetto Flows to relate the entry back to the thread that caused it
  // be logged (typically, the network thread). This bridges the gap between
  // thread call stacks and NetLog, allowing users to correlate them.
  //
  // To provide anchor points for the flow, we write instant events on the
  // current thread stack.
  //
  // For "instant" events (NetLogEventPhase::NONE), we simply generate a random
  // flow ID. The flow starts from the instant event we are writing to the
  // current thread, and terminates on the NetLog event.
  //
  // For non-instant events, it's a bit trickier. For maximum readability, we
  // want the flow to start from the instant event we are writing to the current
  // thread for the BEGIN entry, go through the NetLog event, and then terminate
  // on a separate thread event for the END entry. This means we need to use the
  // same flow ID for BEGIN and END entries. There is no obvious ID we can use
  // that would be identical between the two entries. The approach we use here
  // is to generate the flow ID from the Track ID and the NetLog event type.
  // This will work as long as a given Track doesn't have two NetLog events that
  // are the same type *and* overlap in time. If this assumption breaks, we will
  // need to revisit this approach; we may need to track additional state.
  const auto thread_event_name_str = base::StringPrintf(
      "%s: %s%s/%s", root_track_name_.value,
      [&] {
        switch (entry.phase) {
          case NetLogEventPhase::BEGIN:
            return "BEGIN ";
          case NetLogEventPhase::END:
            return "END ";
          case NetLogEventPhase::NONE:
            return "";
        }
      }(),
      source_type_string.value, entry_type_string.value);
  // Note: the separate variable is load-bearing, as DynamicString will not
  // retain the std::string. See https://crbug.com/417982839.
  const perfetto::DynamicString thread_event_name(thread_event_name_str);
  const uint64_t thread_flow_id =
      entry.phase == NetLogEventPhase::NONE
          ? base::RandUint64()
          : track.uuid + std::hash<std::string_view>()(base::as_string_view(
                             base::byte_span_from_ref(entry.type)));

  if (entry.phase != NetLogEventPhase::END) {
    TRACE_EVENT_INSTANT(kNetLogTracingCategory, thread_event_name,
                        perfetto::Flow::ProcessScoped(thread_flow_id));
  } else {
    TRACE_EVENT_INSTANT(
        kNetLogTracingCategory, thread_event_name,
        perfetto::TerminatingFlow::ProcessScoped(thread_flow_id));
  }
  const auto add_thread_flow = [&](perfetto::EventContext& event_context) {
    switch (entry.phase) {
      case NetLogEventPhase::BEGIN:
        perfetto::Flow::ProcessScoped(thread_flow_id)(event_context);
        break;
      case NetLogEventPhase::END:
        // No need to add the flow: we already added it to this event while
        // processing the BEGIN entry.
        break;
      case NetLogEventPhase::NONE:
        perfetto::TerminatingFlow::ProcessScoped(thread_flow_id)(event_context);
        break;
    }
  };

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

  // We use Perfetto Flows to represent source dependencies; these will show up
  // as arrows in the Perfetto UI. The dependency is on a source, i.e. a track,
  // but Perfetto flows start from an event, not a track. To work around this we
  // write a made-up instant event on the source dependency track to act as an
  // anchor for the flow.
  std::optional<uint64_t> source_dependency_flow_id;
  const base::DictValue* const source_dependency =
      params.FindDict("source_dependency");
  if (source_dependency != nullptr) {
    const std::optional<int> source_dependency_id =
        source_dependency->FindInt("id");
    const std::optional<int> source_dependency_type =
        source_dependency->FindInt("type");
    if (source_dependency_id.has_value() &&
        source_dependency_type.has_value()) {
      source_dependency_flow_id = base::RandUint64();
      CALL_TRACE_EVENT(
          TRACE_EVENT_INSTANT, entry_type_string,
          get_source_track(
              *source_dependency_id,
              SourceTypeToStaticString(
                  static_cast<NetLogSourceType>(*source_dependency_type))),
          perfetto::Flow::ProcessScoped(*source_dependency_flow_id));
    }
  }
  const auto maybe_add_source_dependency_flow =
      [&](perfetto::EventContext& event_context) {
        if (source_dependency_flow_id.has_value()) {
          perfetto::TerminatingFlow::ProcessScoped (*source_dependency_flow_id)(
              event_context);
        }
      };

  const auto set_event_fields = [&](perfetto::EventContext& event_context) {
    add_thread_flow(event_context);
    maybe_add_source_dependency_flow(event_context);
  };

  switch (entry.phase) {
    case NetLogEventPhase::BEGIN:
      CALL_TRACE_EVENT(TRACE_EVENT_BEGIN, entry_type_string, track,
                       set_event_fields, "source_type", source_type_string,
                       "params",
                       std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::END:
      CALL_TRACE_EVENT(TRACE_EVENT_END, track, set_event_fields, "params",
                       std::make_unique<TracedValue>(std::move(params)));
      break;
    case NetLogEventPhase::NONE:
      CALL_TRACE_EVENT(TRACE_EVENT_INSTANT, entry_type_string, track,
                       set_event_fields, "source_type", source_type_string,
                       "params",
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
  if (TRACE_EVENT_CATEGORY_ENABLED(kNetLogTracingCategory)) {
    net_log_to_watch_->AddObserver(this, capture_mode_);
  }
  base::trace_event::TraceSessionObserverList::AddObserver(this);
}

void TraceNetLogObserver::StopWatchForTraceStart() {
  // Should only stop if is currently watching.
  DCHECK(net_log_to_watch_);
  base::trace_event::TraceSessionObserverList::RemoveObserver(this);
  // net_log() != nullptr iff NetLog::AddObserver() has been called.
  // This implies that if the netlog category wasn't enabled, then
  // NetLog::RemoveObserver() will not get called, and there won't be
  // a crash in NetLog::RemoveObserver().
  if (net_log())
    net_log()->RemoveObserver(this);
  net_log_to_watch_ = nullptr;
}

void TraceNetLogObserver::OnStart(const perfetto::DataSourceBase::StartArgs&) {
  if (net_log()) {
    return;
  }
  bool enabled = TRACE_EVENT_CATEGORY_ENABLED(kNetLogTracingCategory);
  if (!enabled)
    return;

  net_log_to_watch_->AddObserver(this, capture_mode_);
}

void TraceNetLogObserver::OnStop(
    const perfetto::DataSourceBase::StopArgs& args) {
  if (!net_log()) {
    return;
  }
  bool should_stop = !base::trace_event::IsCategoryEnabledOnStop(
      PERFETTO_GET_CATEGORY_INDEX(kNetLogTracingCategory), args);
  if (should_stop) {
    net_log()->RemoveObserver(this);
  }
}

}  // namespace net
