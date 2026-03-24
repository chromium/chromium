// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CRASH_REPORT_CONTEXT_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CRASH_REPORT_CONTEXT_AGENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/crash_report_context.h"

namespace blink {

class CrashReportContext;
class InspectedFrames;

class CORE_EXPORT InspectorCrashReportContextAgent
    : public InspectorBaseAgent<protocol::CrashReportContext::Metainfo> {
 public:
  explicit InspectorCrashReportContextAgent(InspectedFrames*);
  InspectorCrashReportContextAgent(const InspectorCrashReportContextAgent&) =
      delete;
  InspectorCrashReportContextAgent& operator=(
      const InspectorCrashReportContextAgent&) = delete;
  ~InspectorCrashReportContextAgent() override;
  void Trace(Visitor*) const override;

  // Protocol methods.
  protocol::Response getEntries(
      std::unique_ptr<protocol::Array<
          protocol::CrashReportContext::CrashReportContextEntry>>* entries)
      override;

 private:
  Member<InspectedFrames> inspected_frames_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_CRASH_REPORT_CONTEXT_AGENT_H_
