// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PERFORMANCE_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PERFORMANCE_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Performance.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class InspectedFrames;

namespace probe {
class CallFunction;
class ExecuteScript;
class RecalculateStyle;
class UpdateLayout;
class V8Compile;
}  // namespace probe

class CORE_EXPORT InspectorPerformanceAgent final
    : public InspectorBaseAgent<protocol::Performance::Metainfo>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  void Trace(blink::Visitor*) override;

  static InspectorPerformanceAgent* Create(InspectedFrames* inspected_frames) {
    return new InspectorPerformanceAgent(inspected_frames);
  }
  ~InspectorPerformanceAgent() override;

  void Restore() override;

  // Performance protocol domain implementation.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response setTimeDomain(const String& time_domain) override;
  protocol::Response getMetrics(
      std::unique_ptr<protocol::Array<protocol::Performance::Metric>>*
          out_result) override;

  // PerformanceMetrics probes implementation.
  void ConsoleTimeStamp(const String& title);
  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);
  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);
  void Will(const probe::V8Compile&);
  void Did(const probe::V8Compile&);

  // TaskTimeObserver implementation.
  void WillProcessTask(base::TimeTicks start_time) override;
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override;

 private:
  explicit InspectorPerformanceAgent(InspectedFrames*);
  void ScriptStarts();
  void ScriptEnds();
  void InnerEnable();
  TimeTicks GetTimeTicksNow();

  Member<InspectedFrames> inspected_frames_;
  TimeDelta layout_duration_;
  TimeTicks layout_start_ticks_;
  TimeDelta recalc_style_duration_;
  TimeTicks recalc_style_start_ticks_;
  TimeDelta script_duration_;
  TimeTicks script_start_ticks_;
  TimeDelta task_duration_;
  TimeTicks task_start_ticks_;
  TimeDelta v8compile_duration_;
  TimeTicks v8compile_start_ticks_;
  unsigned long long layout_count_ = 0;
  unsigned long long recalc_style_count_ = 0;
  int script_call_depth_ = 0;
  int layout_depth_ = 0;
  bool use_thread_ticks_ = false;
  InspectorAgentState::Boolean enabled_;
  DISALLOW_COPY_AND_ASSIGN(InspectorPerformanceAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_PERFORMANCE_AGENT_H_
