// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/performance_monitor.h"

#include "base/format_macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "v8/include/v8-metrics.h"

namespace blink {

// static
base::TimeDelta PerformanceMonitor::Threshold(ExecutionContext* context,
                                              Violation violation) {
  // Calling InstrumentingMonitorExcludingLongTasks wouldn't work properly if
  // this query is for longtasks.
  DCHECK(violation != kLongTask);
  PerformanceMonitor* monitor =
      PerformanceMonitor::InstrumentingMonitorExcludingLongTasks(context);
  return monitor ? monitor->thresholds_[violation] : base::TimeDelta();
}

// static
void PerformanceMonitor::ReportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    base::TimeDelta time,
    std::unique_ptr<SourceLocation> location) {
  // Calling InstrumentingMonitorExcludingLongTasks wouldn't work properly if
  // this is a longtask violation.
  DCHECK(violation != kLongTask);
  PerformanceMonitor* monitor =
      PerformanceMonitor::InstrumentingMonitorExcludingLongTasks(context);
  if (!monitor)
    return;
  monitor->InnerReportGenericViolation(context, violation, text, time,
                                       std::move(location));
}

// static
PerformanceMonitor* PerformanceMonitor::Monitor(
    const ExecutionContext* context) {
  const auto* window = DynamicTo<LocalDOMWindow>(context);
  if (!window)
    return nullptr;
  LocalFrame* frame = window->GetFrame();
  if (!frame)
    return nullptr;
  return frame->GetPerformanceMonitor();
}

// static
PerformanceMonitor* PerformanceMonitor::InstrumentingMonitorExcludingLongTasks(
    const ExecutionContext* context) {
  PerformanceMonitor* monitor = PerformanceMonitor::Monitor(context);
  return monitor && monitor->enabled_ ? monitor : nullptr;
}

PerformanceMonitor::PerformanceMonitor(LocalFrame* local_root,
                                       v8::Isolate* isolate)
    : local_root_(local_root), isolate_(isolate) {
  std::fill(std::begin(thresholds_), std::end(thresholds_), base::TimeDelta());
  Thread::Current()->AddTaskTimeObserver(this);
  local_root_->GetProbeSink()->AddPerformanceMonitor(this);
}

PerformanceMonitor::~PerformanceMonitor() {
  DCHECK(!local_root_);
}

void PerformanceMonitor::Dispose() {
  if (!was_shutdown_) {
    // `PerformanceMonitor` should never be deleted without having been
    // `Shutdown()`. As a temporary workaround for crbug.com/337200890,
    // unregister as a `TaskTimeObserver` if `Shutdown()` wasn't called.
    //
    // TODO(crbug.com/337200890): Remove when the root cause of the bug has been
    // addressed.
    Thread::Current()->RemoveTaskTimeObserver(this);
  }
}

void PerformanceMonitor::Subscribe(Violation violation,
                                   base::TimeDelta threshold,
                                   Client* client) {
  DCHECK(violation < kAfterLast);
  ClientThresholds* client_thresholds = nullptr;

  auto it = subscriptions_.find(violation);
  if (it == subscriptions_.end()) {
    client_thresholds = MakeGarbageCollected<ClientThresholds>();
    subscriptions_.Set(violation, client_thresholds);
  } else {
    client_thresholds = it->value;
  }

  client_thresholds->Set(client, threshold);
  UpdateInstrumentation();
}

void PerformanceMonitor::UnsubscribeAll(Client* client) {
  for (const auto& it : subscriptions_)
    it.value->erase(client);
  UpdateInstrumentation();
}

void PerformanceMonitor::Shutdown() {
  if (!local_root_)
    return;
  subscriptions_.clear();
  UpdateInstrumentation();
  Thread::Current()->RemoveTaskTimeObserver(this);
  local_root_->GetProbeSink()->RemovePerformanceMonitor(this);
  local_root_ = nullptr;
  was_shutdown_ = true;
}

void PerformanceMonitor::UpdateInstrumentation() {
  std::fill(std::begin(thresholds_), std::end(thresholds_), base::TimeDelta());

  for (const auto& it : subscriptions_) {
    Violation violation = static_cast<Violation>(it.key);
    ClientThresholds* client_thresholds = it.value;
    for (const auto& client_threshold : *client_thresholds) {
      if (thresholds_[violation].is_zero() ||
          thresholds_[violation] > client_threshold.value)
        thresholds_[violation] = client_threshold.value;
    }
  }

  static_assert(kLongTask == 0u,
                "kLongTask should be the first value in Violation for the "
                "|enabled_| definition below to be correct");
  // Since kLongTask is the first in |thresholds_|, we count from one after
  // begin(thresholds_).
  enabled_ = std::count(std::begin(thresholds_) + 1, std::end(thresholds_),
                        base::TimeDelta()) < static_cast<int>(kAfterLast) - 1;
}

void PerformanceMonitor::WillExecuteScript(ExecutionContext* context) {
  // Heuristic for minimal frame context attribution: note the frame context
  // for each script execution. When a long task is encountered,
  // if there is only one frame context involved, then report it.
  // Otherwise don't report frame context.
  // NOTE: This heuristic is imperfect and will be improved in V2 API.
  // In V2, timing of script execution along with style & layout updates will be
  // accounted for detailed and more accurate attribution.
  ++script_depth_;
  UpdateTaskAttribution(context);
}

void PerformanceMonitor::DidExecuteScript() {
  --script_depth_;
}

void PerformanceMonitor::UpdateTaskAttribution(ExecutionContext* context) {
  // If |context| is not a window, unable to attribute a frame context.
  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (!window)
    return;

  UpdateTaskShouldBeReported(window->GetFrame());
  if (!task_execution_context_)
    task_execution_context_ = context;
  else if (task_execution_context_ != context)
    task_has_multiple_contexts_ = true;
}

void PerformanceMonitor::UpdateTaskShouldBeReported(LocalFrame* frame) {
  if (frame && local_root_ == &(frame->LocalFrameRoot()))
    task_should_be_reported_ = true;
}

void PerformanceMonitor::Will(const probe::RecalculateStyle& probe) {
  UpdateTaskShouldBeReported(probe.document ? probe.document->GetFrame()
                                            : nullptr);
  if (enabled_ && !thresholds_[kLongLayout].is_zero() && script_depth_) {
    probe.CaptureStartTime();
  }
}

void PerformanceMonitor::Did(const probe::RecalculateStyle& probe) {
  if (enabled_ && script_depth_ && !thresholds_[kLongLayout].is_zero()) {
    per_task_style_and_layout_time_ += probe.Duration();
  }
}

void PerformanceMonitor::Will(const probe::UpdateLayout& probe) {
  UpdateTaskShouldBeReported(probe.document ? probe.document->GetFrame()
                                            : nullptr);
  ++layout_depth_;
  if (!enabled_)
    return;
  if (layout_depth_ > 1 || !script_depth_ || thresholds_[kLongLayout].is_zero())
    return;

  probe.CaptureStartTime();
}

void PerformanceMonitor::Did(const probe::UpdateLayout& probe) {
  --layout_depth_;
  if (!enabled_)
    return;
  if (!thresholds_[kLongLayout].is_zero() && script_depth_ && !layout_depth_)
    per_task_style_and_layout_time_ += probe.Duration();
}

void PerformanceMonitor::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.context);
}

void PerformanceMonitor::Did(const probe::ExecuteScript& probe) {
  DidExecuteScript();
}

void PerformanceMonitor::Will(const probe::CallFunction& probe) {
  WillExecuteScript(probe.context);
  if (user_callback_)
    probe.CaptureStartTime();
}

void PerformanceMonitor::Did(const probe::CallFunction& probe) {
  DidExecuteScript();
  if (!enabled_ || !user_callback_)
    return;

  // Working around Oilpan - probes are STACK_ALLOCATED.
  const probe::UserCallback* user_callback =
      static_cast<const probe::UserCallback*>(user_callback_);
  Violation handler_type =
      user_callback->recurring ? kRecurringHandler : kHandler;
  base::TimeDelta threshold = thresholds_[handler_type];
  base::TimeDelta duration = probe.Duration();
  if (threshold.is_zero() || duration < threshold)
    return;

  String name = user_callback->name ? String(user_callback->name)
                                    : String(user_callback->atomic_name);
  String text = String::Format("'%s' handler took %" PRId64 "ms",
                               name.Utf8().c_str(), duration.InMilliseconds());
  InnerReportGenericViolation(
      probe.context, handler_type, text, duration,
      CaptureSourceLocation(probe.context->GetIsolate(), probe.function));
}

void PerformanceMonitor::Will(const probe::V8Compile& probe) {
  UpdateTaskAttribution(probe.context);
}

void PerformanceMonitor::Did(const probe::V8Compile& probe) {}

void PerformanceMonitor::Will(const probe::UserCallback& probe) {
  ++user_callback_depth_;
  UpdateTaskAttribution(probe.context);
  if (!enabled_ || user_callback_depth_ != 1 ||
      thresholds_[probe.recurring ? kRecurringHandler : kHandler].is_zero())
    return;

  DCHECK(!user_callback_);
  user_callback_ = &probe;
}

void PerformanceMonitor::Did(const probe::UserCallback& probe) {
  --user_callback_depth_;
  if (!user_callback_depth_)
    user_callback_ = nullptr;
  DCHECK(user_callback_ != &probe);
}

void PerformanceMonitor::DocumentWriteFetchScript(Document* document) {
  if (!enabled_)
    return;
  String text = "Parser was blocked due to document.write(<script>)";
  InnerReportGenericViolation(document->GetExecutionContext(), kBlockedParser,
                              text, base::TimeDelta(), nullptr);
}

void PerformanceMonitor::WillProcessTask(base::TimeTicks start_time) {
  // Reset m_taskExecutionContext. We don't clear this in didProcessTask
  // as it is needed in ReportTaskTime which occurs after didProcessTask.
  // Always reset variables needed for longtasks, regardless of the value of
  // |enabled_|.
  task_execution_context_ = nullptr;
  task_has_multiple_contexts_ = false;
  task_should_be_reported_ = false;
  v8::metrics::LongTaskStats::Reset(isolate_);

  if (!enabled_)
    return;

  // Reset everything for regular and nested tasks.
  script_depth_ = 0;
  layout_depth_ = 0;
  per_task_style_and_layout_time_ = base::TimeDelta();
  user_callback_ = nullptr;
}

void PerformanceMonitor::DidProcessTask(base::TimeTicks start_time,
                                        base::TimeTicks end_time) {
  if (!task_should_be_reported_)
    return;

  // Do not check the value of |enabled_| before processing longtasks.
  // |enabled_| can be false while there are subscriptions to longtask
  // violations.
  if (!thresholds_[kLongTask].is_zero()) {
    base::TimeDelta task_time = end_time - start_time;
    if (task_time > thresholds_[kLongTask]) {
      auto subscriptions_it = subscriptions_.find(kLongTask);
      if (subscriptions_it != subscriptions_.end()) {
        ClientThresholds* client_thresholds = subscriptions_it->value;
        DCHECK(client_thresholds);

        for (const auto& it : *client_thresholds) {
          if (it.value < task_time) {
            it.key->ReportLongTask(
                start_time, end_time,
                task_has_multiple_contexts_ ? nullptr : task_execution_context_,
                task_has_multiple_contexts_);
          }
        }
      }
    }
  }

  if (!enabled_)
    return;

  base::TimeDelta layout_threshold = thresholds_[kLongLayout];
  base::TimeDelta layout_time = per_task_style_and_layout_time_;
  if (!layout_threshold.is_zero() && layout_time > layout_threshold) {
    ClientThresholds* client_thresholds = subscriptions_.at(kLongLayout);
    DCHECK(client_thresholds);
    for (const auto& it : *client_thresholds) {
      if (it.value < layout_time)
        it.key->ReportLongLayout(layout_time);
    }
  }
}

void PerformanceMonitor::InnerReportGenericViolation(
    ExecutionContext* context,
    Violation violation,
    const String& text,
    base::TimeDelta time,
    std::unique_ptr<SourceLocation> location) {
  auto subscriptions_it = subscriptions_.find(violation);
  if (subscriptions_it == subscriptions_.end())
    return;

  if (!location)
    location = CaptureSourceLocation(context);

  ClientThresholds* client_thresholds = subscriptions_it->value;
  for (const auto& it : *client_thresholds) {
    if (it.value < time)
      it.key->ReportGenericViolation(violation, text, time, location.get());
  }
}

void PerformanceMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(local_root_);
  visitor->Trace(task_execution_context_);
  visitor->Trace(subscriptions_);
}

}  // namespace blink
