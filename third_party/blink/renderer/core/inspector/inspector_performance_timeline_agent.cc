// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_performance_timeline_agent.h"

#include <utility>

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/layout_shift.h"
#include "third_party/blink/renderer/core/timing/layout_shift_attribution.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"

namespace blink {

namespace {

constexpr PerformanceEntryType kSupportedTypes =
    PerformanceEntry::EntryType::kLargestContentfulPaint |
    PerformanceEntry::EntryType::kLayoutShift;

double ToProtocolTime(DOMHighResTimeStamp timeOrigin,
                      DOMHighResTimeStamp time) {
  return time ? ConvertDOMHighResTimeStampToSeconds(timeOrigin + time) : 0.0;
}

std::unique_ptr<protocol::PerformanceTimeline::LargestContentfulPaint>
BuildEventDetails(const LargestContentfulPaint& lcp,
                  DOMHighResTimeStamp timeOrigin) {
  auto result = protocol::PerformanceTimeline::LargestContentfulPaint::create()
                    .setRenderTime(ToProtocolTime(timeOrigin, lcp.renderTime()))
                    .setLoadTime(ToProtocolTime(timeOrigin, lcp.loadTime()))
                    .setSize(lcp.size())
                    .build();
  if (!lcp.id().empty())
    result->setElementId(lcp.id());
  if (Element* element = lcp.element())
    result->setNodeId(IdentifiersFactory::IntIdForNode(element));
  if (!lcp.url().empty())
    result->setUrl(lcp.url());
  return result;
}

std::unique_ptr<protocol::DOM::Rect> BuildRect(DOMRectReadOnly* rect) {
  return protocol::DOM::Rect::create()
      .setX(rect->x())
      .setY(rect->y())
      .setWidth(rect->width())
      .setHeight(rect->height())
      .build();
}

std::unique_ptr<
    protocol::Array<protocol::PerformanceTimeline::LayoutShiftAttribution>>
BuildLayoutShiftAttributions(const LayoutShift::AttributionList& attributions) {
  auto result = std::make_unique<
      protocol::Array<protocol::PerformanceTimeline::LayoutShiftAttribution>>();
  for (const auto& attr : attributions) {
    auto entry = protocol::PerformanceTimeline::LayoutShiftAttribution::create()
                     .setPreviousRect(BuildRect(attr->previousRect()))
                     .setCurrentRect(BuildRect(attr->currentRect()))
                     .build();
    if (Node* node = attr->rawNodeForInspector())
      entry->setNodeId(IdentifiersFactory::IntIdForNode(node));
    result->push_back(std::move(entry));
  }
  return result;
}

std::unique_ptr<protocol::PerformanceTimeline::LayoutShift> BuildEventDetails(
    const LayoutShift& ls,
    DOMHighResTimeStamp timeOrigin) {
  auto result =
      protocol::PerformanceTimeline::LayoutShift::create()
          .setValue(ls.value())
          .setHadRecentInput(ls.hadRecentInput())
          .setLastInputTime(ToProtocolTime(timeOrigin, ls.lastInputTime()))
          .setSources(BuildLayoutShiftAttributions(ls.sources()))
          .build();
  return result;
}

std::unique_ptr<protocol::PerformanceTimeline::TimelineEvent>
BuildProtocolEvent(String frame_id,
                   DOMHighResTimeStamp timeOrigin,
                   PerformanceEntry* entry) {
  auto result = protocol::PerformanceTimeline::TimelineEvent::create()
                    .setFrameId(frame_id)
                    .setType(entry->entryType())
                    .setName(entry->name())
                    // TODO(caseq): entry time is clamped; consider exposing an
                    // unclamped time.
                    .setTime(ToProtocolTime(timeOrigin, entry->startTime()))
                    .build();
  if (entry->duration())
    result->setDuration(ConvertDOMHighResTimeStampToSeconds(entry->duration()));
  if (auto* lcp = DynamicTo<LargestContentfulPaint>(entry))
    result->setLcpDetails(BuildEventDetails(*lcp, timeOrigin));
  else if (auto* ls = DynamicTo<LayoutShift>(entry))
    result->setLayoutShiftDetails(BuildEventDetails(*ls, timeOrigin));
  return result;
}

}  // namespace

InspectorPerformanceTimelineAgent::InspectorPerformanceTimelineAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      enabled_types_(&agent_state_, /*default_value=*/false) {}

InspectorPerformanceTimelineAgent::~InspectorPerformanceTimelineAgent() =
    default;

void InspectorPerformanceTimelineAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent<protocol::PerformanceTimeline::Metainfo>::Trace(visitor);
}

void InspectorPerformanceTimelineAgent::Restore() {
  if (IsEnabled())
    InnerEnable();
}

void InspectorPerformanceTimelineAgent::InnerEnable() {
  DCHECK(IsEnabled());
  instrumenting_agents_->AddInspectorPerformanceTimelineAgent(this);
}

void InspectorPerformanceTimelineAgent::PerformanceEntryAdded(
    ExecutionContext* context,
    PerformanceEntry* entry) {
  if (!(entry->EntryTypeEnum() & enabled_types_.Get()))
    return;
  String frame_id;
  Performance* performance = nullptr;
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    frame_id = IdentifiersFactory::FrameId(window->GetFrame());
    performance = DOMWindowPerformance::performance(*window);
  } else if (auto* global_scope = DynamicTo<WorkerGlobalScope>(context)) {
    performance = WorkerGlobalScopePerformance::performance(*global_scope);
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected subtype of ExecutionContext";
  }
  GetFrontend()->timelineEventAdded(
      BuildProtocolEvent(frame_id, performance->timeOrigin(), entry));
}

protocol::Response InspectorPerformanceTimelineAgent::enable(
    std::unique_ptr<protocol::Array<String>> entry_types) {
  EventsVector buffered_events;

  const int old_types = enabled_types_.Get();
  PerformanceEntryType new_types = 0;
  for (const auto& type_str : *entry_types) {
    AtomicString type_atomic(type_str);
    PerformanceEntryType type_enum =
        PerformanceEntry::ToEntryTypeEnum(type_atomic);
    if (type_enum == PerformanceEntry::EntryType::kInvalid ||
        (type_enum & kSupportedTypes) != type_enum) {
      return protocol::Response::InvalidParams(
          "Unknown or unsupported entry type");
    }

    // Gather buffered entries for types that haven't been enabled previously
    // (but disregard duplicate type specifiers).
    if (!(old_types & type_enum) && !(new_types & type_enum))
      CollectEntries(type_atomic, &buffered_events);
    new_types |= type_enum;
  }
  enabled_types_.Set(new_types);
  if (!old_types != !new_types) {
    if (!new_types)
      return disable();
    InnerEnable();
  }
  for (auto& event : buffered_events)
    GetFrontend()->timelineEventAdded(std::move(event));

  return protocol::Response::Success();
}

protocol::Response InspectorPerformanceTimelineAgent::disable() {
  enabled_types_.Clear();
  instrumenting_agents_->RemoveInspectorPerformanceTimelineAgent(this);
  return protocol::Response::Success();
}

bool InspectorPerformanceTimelineAgent::IsEnabled() const {
  return !!enabled_types_.Get();
}

void InspectorPerformanceTimelineAgent::CollectEntries(AtomicString type,
                                                       EventsVector* events) {
  for (LocalFrame* frame : *inspected_frames_) {
    String frame_id = IdentifiersFactory::FrameId(frame);
    LocalDOMWindow* window = frame->DomWindow();
    if (!window)
      continue;
    WindowPerformance* performance = DOMWindowPerformance::performance(*window);
    for (Member<PerformanceEntry> entry :
         performance->getBufferedEntriesByType(type)) {
      events->push_back(
          BuildProtocolEvent(frame_id, performance->timeOrigin(), entry));
    }
  }
}

}  // namespace blink
