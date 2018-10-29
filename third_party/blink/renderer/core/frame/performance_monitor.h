// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PERFORMANCE_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PERFORMANCE_MONITOR_H_

#include "base/macros.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/timing/sub_task_attribution.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8.h"

namespace blink {

namespace probe {
class CallFunction;
class ExecuteScript;
class RecalculateStyle;
class UpdateLayout;
class UserCallback;
class V8Compile;
}  // namespace probe

class DOMWindow;
class Document;
class ExecutionContext;
class WindowPerformance;
class SourceLocation;

// Performance monitor for Web Performance APIs and logging.
// The monitor is maintained per local root.
// Long task notifications are delivered to observing WindowPerformance*
// instances (in the local frame tree) in m_webPerformanceObservers.
class CORE_EXPORT PerformanceMonitor final
    : public GarbageCollectedFinalized<PerformanceMonitor>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  enum Violation : size_t {
    kLongTask,
    kLongLayout,
    kBlockedEvent,
    kBlockedParser,
    kDiscouragedAPIUse,
    kHandler,
    kRecurringHandler,
    kAfterLast
  };

  class CORE_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual void ReportLongTask(
        base::TimeTicks start_time,
        base::TimeTicks end_time,
        ExecutionContext* task_context,
        bool has_multiple_contexts,
        const SubTaskAttribution::EntriesVector& sub_task_attributions) {}
    virtual void ReportLongLayout(base::TimeDelta duration) {}
    virtual void ReportGenericViolation(Violation,
                                        const String& text,
                                        base::TimeDelta time,
                                        SourceLocation*) {}
    void Trace(blink::Visitor* visitor) override {}
  };

  static void ReportGenericViolation(ExecutionContext*,
                                     Violation,
                                     const String& text,
                                     base::TimeDelta time,
                                     std::unique_ptr<SourceLocation>);
  static base::TimeDelta Threshold(ExecutionContext*, Violation);

  void BypassLongCompileThresholdOnceForTesting();

  // Instrumenting methods.
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);

  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);

  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);

  void Will(const probe::CallFunction&);
  void Did(const probe::CallFunction&);

  void Will(const probe::UserCallback&);
  void Did(const probe::UserCallback&);

  void Will(const probe::V8Compile&);
  void Did(const probe::V8Compile&);

  void DomContentLoadedEventFired(LocalFrame*);

  void DocumentWriteFetchScript(Document*);

  // Direct API for core.
  void Subscribe(Violation, base::TimeDelta threshold, Client*);
  void UnsubscribeAll(Client*);
  void Shutdown();

  explicit PerformanceMonitor(LocalFrame*);
  ~PerformanceMonitor() override;

  virtual void Trace(blink::Visitor*);

 private:
  friend class PerformanceMonitorTest;
  friend class WindowPerformanceTest;

  static PerformanceMonitor* Monitor(const ExecutionContext*);
  static PerformanceMonitor* InstrumentingMonitor(const ExecutionContext*);

  void UpdateInstrumentation();

  void InnerReportGenericViolation(ExecutionContext*,
                                   Violation,
                                   const String& text,
                                   base::TimeDelta time,
                                   std::unique_ptr<SourceLocation>);

  // TaskTimeObserver implementation
  void WillProcessTask(base::TimeTicks start_time) override;
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override;

  void WillExecuteScript(ExecutionContext*);
  void DidExecuteScript();

  void UpdateTaskAttribution(ExecutionContext*);
  void UpdateTaskShouldBeReported(LocalFrame*);

  std::pair<String, DOMWindow*> SanitizedAttribution(
      const HeapHashSet<Member<Frame>>& frame_contexts,
      Frame* observer_frame);

  bool enabled_ = false;
  TimeDelta per_task_style_and_layout_time_;
  unsigned script_depth_ = 0;
  unsigned layout_depth_ = 0;
  unsigned user_callback_depth_ = 0;
  const void* user_callback_;
  TimeTicks v8_compile_start_time_;

  SubTaskAttribution::EntriesVector sub_task_attributions_;

  base::TimeDelta thresholds_[kAfterLast];

  Member<LocalFrame> local_root_;
  Member<ExecutionContext> task_execution_context_;
  bool task_has_multiple_contexts_ = false;
  bool task_should_be_reported_ = false;
  using ClientThresholds = HeapHashMap<WeakMember<Client>, base::TimeDelta>;
  HeapHashMap<Violation,
              Member<ClientThresholds>,
              typename DefaultHash<size_t>::Hash,
              WTF::UnsignedWithZeroKeyHashTraits<size_t>>
      subscriptions_;
  bool bypass_long_compile_threshold_ = false;

  DISALLOW_COPY_AND_ASSIGN(PerformanceMonitor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PERFORMANCE_MONITOR_H_
