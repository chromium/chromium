// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_crash_report_context_agent.h"

#include "third_party/blink/renderer/core/frame/crash_report_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

InspectorCrashReportContextAgent::InspectorCrashReportContextAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorCrashReportContextAgent::~InspectorCrashReportContextAgent() = default;

void InspectorCrashReportContextAgent::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

protocol::Response InspectorCrashReportContextAgent::getEntries(
    std::unique_ptr<
        protocol::Array<protocol::CrashReportContext::CrashReportContextEntry>>*
        entries) {
  auto entry_array = std::make_unique<
      protocol::Array<protocol::CrashReportContext::CrashReportContextEntry>>();

  for (LocalFrame* frame : *inspected_frames_) {
    LocalDOMWindow* window = frame->DomWindow();
    if (!window) {
      continue;
    }

    if (!RuntimeEnabledFeatures::CrashReportingStorageAPIEnabled(
            window->GetExecutionContext())) {
      continue;
    }

    auto* crash_report_context = window->crashReport();
    if (crash_report_context) {
      String frame_id = IdentifiersFactory::FrameId(frame);
      const auto& context_map =
          crash_report_context->InternalContextForInspection();
      for (const auto& pair : context_map) {
        entry_array->emplace_back(
            protocol::CrashReportContext::CrashReportContextEntry::create()
                .setKey(pair.key)
                .setValue(pair.value)
                .setFrameId(frame_id)
                .build());
      }
    }
  }

  *entries = std::move(entry_array);
  return protocol::Response::Success();
}

}  // namespace blink
