/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/script/script_runner.h"

#include <algorithm>
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

ScriptRunner::ScriptRunner(Document* document)
    : ExecutionContextLifecycleStateObserver(document->GetExecutionContext()),
      document_(document),
      task_runner_(document->GetTaskRunner(TaskType::kNetworking)) {
  DCHECK(document);
  UpdateStateIfNeeded();
}

void ScriptRunner::QueueScriptForExecution(PendingScript* pending_script) {
  DCHECK(pending_script);
  document_->IncrementLoadEventDelayCount();
  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      pending_async_scripts_.insert(pending_script);
      break;

    case ScriptSchedulingType::kInOrder:
      pending_in_order_scripts_.push_back(pending_script);
      number_of_in_order_scripts_with_pending_notification_++;
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ScriptRunner::PostTask(const base::Location& web_trace_location) {
  task_runner_->PostTask(
      web_trace_location,
      WTF::Bind(&ScriptRunner::ExecuteTask, WrapWeakPersistent(this)));
}

void ScriptRunner::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (!IsExecutionSuspended())
    PostTasksForReadyScripts(FROM_HERE);
}

bool ScriptRunner::IsExecutionSuspended() {
  return !GetExecutionContext() || GetExecutionContext()->IsContextPaused();
}

void ScriptRunner::PostTasksForReadyScripts(
    const base::Location& web_trace_location) {
  DCHECK(!IsExecutionSuspended());

  for (size_t i = 0; i < async_scripts_to_execute_soon_.size(); ++i) {
    PostTask(web_trace_location);
  }
  for (size_t i = 0; i < in_order_scripts_to_execute_soon_.size(); ++i) {
    PostTask(web_trace_location);
  }
}

void ScriptRunner::ScheduleReadyInOrderScripts() {
  while (!pending_in_order_scripts_.IsEmpty() &&
         pending_in_order_scripts_.front()
             ->IsReady()) {
    in_order_scripts_to_execute_soon_.push_back(
        pending_in_order_scripts_.TakeFirst());
    PostTask(FROM_HERE);
  }
}

void ScriptRunner::DelayAsyncScriptUntilMilestoneReached(
    PendingScript* pending_script) {
  DCHECK(!delay_async_script_milestone_reached_);
  SECURITY_CHECK(pending_async_scripts_.Contains(pending_script));
  pending_async_scripts_.erase(pending_script);

  // When the ScriptRunner is notified via
  // |NotifyDelayedAsyncScriptsMilestoneReached()|, the scripts in
  // |pending_delayed_async_scripts_| will be scheduled for execution.
  pending_delayed_async_scripts_.push_back(pending_script);
}

void ScriptRunner::NotifyDelayedAsyncScriptsMilestoneReached() {
  delay_async_script_milestone_reached_ = true;
  while (!pending_delayed_async_scripts_.IsEmpty()) {
    PendingScript* pending_script = pending_delayed_async_scripts_.TakeFirst();
    DCHECK_EQ(pending_script->GetSchedulingType(),
              ScriptSchedulingType::kAsync);

    async_scripts_to_execute_soon_.push_back(pending_script);
    PostTask(FROM_HERE);
  }
}

bool ScriptRunner::CanDelayAsyncScripts() {
  if (delay_async_script_milestone_reached_)
    return false;

  // We first check to see if the base::Feature is enabled, before the
  // RuntimeEnabledFeatures. This is because the RuntimeEnabledFeatures simply
  // exist for testing, so if they are enabled *and* the base::Feature is
  // enabled, we should log UKM via DocumentLoader::DidObserveLoadingBehavior,
  // which is associated with the experiment running the base::Feature flag.
  static bool feature_enabled =
      base::FeatureList::IsEnabled(features::kDelayAsyncScriptExecution);
  bool optimization_guide_hints_unknown =
      !document_->GetFrame() ||
      !document_->GetFrame()->GetOptimizationGuideHints() ||
      !document_->GetFrame()
           ->GetOptimizationGuideHints()
           ->delay_async_script_execution_hints ||
      document_->GetFrame()
              ->GetOptimizationGuideHints()
              ->delay_async_script_execution_hints->delay_type ==
          mojom::blink::DelayAsyncScriptExecutionDelayType::kUnknown;
  if (feature_enabled) {
    if (document_->Parsing() && document_->Loader()) {
      document_->Loader()->DidObserveLoadingBehavior(
          kLoadingBehaviorAsyncScriptReadyBeforeDocumentFinishedParsing);
    }

    // If the base::Feature is enabled, we always want to delay async scripts,
    // unless we delegate to the OptimizationGuide, but the hints aren't
    // available.
    if (features::kDelayAsyncScriptExecutionDelayParam.Get() !=
            features::DelayAsyncScriptDelayType::kUseOptimizationGuide ||
        !optimization_guide_hints_unknown) {
      return true;
    }
  }

  // Delay milestone has not been reached yet. We have to check the feature flag
  // configuration to see if we are able to delay async scripts or not:
  if (RuntimeEnabledFeatures::
          DelayAsyncScriptExecutionUntilFinishedParsingEnabled() ||
      RuntimeEnabledFeatures::
          DelayAsyncScriptExecutionUntilFirstPaintOrFinishedParsingEnabled()) {
    return true;
  }

  return false;
}

void ScriptRunner::NotifyScriptReady(PendingScript* pending_script) {
  SECURITY_CHECK(pending_script);

  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      // SECURITY_CHECK() makes us crash in a controlled way in error cases
      // where the PendingScript is associated with the wrong ScriptRunner
      // (otherwise we'd cause a use-after-free in ~ScriptRunner when it tries
      // to detach).
      SECURITY_CHECK(pending_async_scripts_.Contains(pending_script));

      if (pending_script->IsEligibleForDelay() && CanDelayAsyncScripts()) {
        DelayAsyncScriptUntilMilestoneReached(pending_script);
        return;
      }

      pending_async_scripts_.erase(pending_script);
      async_scripts_to_execute_soon_.push_back(pending_script);

      PostTask(FROM_HERE);
      break;

    case ScriptSchedulingType::kInOrder:
      SECURITY_CHECK(number_of_in_order_scripts_with_pending_notification_ > 0);
      number_of_in_order_scripts_with_pending_notification_--;

      ScheduleReadyInOrderScripts();

      break;

    default:
      NOTREACHED();
      break;
  }
}

bool ScriptRunner::RemovePendingInOrderScript(PendingScript* pending_script) {
  auto it = std::find(pending_in_order_scripts_.begin(),
                      pending_in_order_scripts_.end(), pending_script);
  if (it == pending_in_order_scripts_.end())
    return false;
  pending_in_order_scripts_.erase(it);
  SECURITY_CHECK(number_of_in_order_scripts_with_pending_notification_ > 0);
  number_of_in_order_scripts_with_pending_notification_--;
  return true;
}

void ScriptRunner::MovePendingScript(Document& old_document,
                                     Document& new_document,
                                     ScriptLoader* script_loader) {
  Document* new_context_document =
      new_document.GetExecutionContext()
          ? To<LocalDOMWindow>(new_document.GetExecutionContext())->document()
          : &new_document;
  Document* old_context_document =
      old_document.GetExecutionContext()
          ? To<LocalDOMWindow>(old_document.GetExecutionContext())->document()
          : &old_document;
  if (old_context_document == new_context_document)
    return;

  PendingScript* pending_script =
      script_loader
          ->GetPendingScriptIfControlledByScriptRunnerForCrossDocMove();
  if (!pending_script) {
    // The ScriptLoader is not controlled by ScriptRunner. This can happen
    // because MovePendingScript() is called for all <script> elements
    // moved between Documents, not only for those controlled by ScriptRunner.
    return;
  }

  old_context_document->GetScriptRunner()->MovePendingScript(
      new_context_document->GetScriptRunner(), pending_script);
}

void ScriptRunner::MovePendingScript(ScriptRunner* new_runner,
                                     PendingScript* pending_script) {
  auto it = pending_async_scripts_.find(pending_script);
  if (it != pending_async_scripts_.end()) {
    new_runner->QueueScriptForExecution(pending_script);
    pending_async_scripts_.erase(it);
    document_->DecrementLoadEventDelayCount();
    return;
  }
  if (RemovePendingInOrderScript(pending_script)) {
    new_runner->QueueScriptForExecution(pending_script);
    document_->DecrementLoadEventDelayCount();
  }
}

bool ScriptRunner::ExecuteInOrderTask() {
  TRACE_EVENT0("blink", "ScriptRunner::ExecuteInOrderTask");
  if (in_order_scripts_to_execute_soon_.IsEmpty())
    return false;

  PendingScript* pending_script = in_order_scripts_to_execute_soon_.TakeFirst();
  DCHECK(pending_script);
  DCHECK_EQ(pending_script->GetSchedulingType(), ScriptSchedulingType::kInOrder)
      << "In-order scripts queue should not contain any async script.";

  pending_script->ExecuteScriptBlock(NullURL());

  document_->DecrementLoadEventDelayCount();
  return true;
}

bool ScriptRunner::ExecuteAsyncTask() {
  TRACE_EVENT0("blink", "ScriptRunner::ExecuteAsyncTask");
  if (async_scripts_to_execute_soon_.IsEmpty())
    return false;

  // Remove the async script loader from the ready-to-exec set and execute.
  PendingScript* pending_script = async_scripts_to_execute_soon_.TakeFirst();

  DCHECK_EQ(pending_script->GetSchedulingType(), ScriptSchedulingType::kAsync)
      << "Async scripts queue should not contain any in-order script.";

  pending_script->ExecuteScriptBlock(NullURL());

  document_->DecrementLoadEventDelayCount();
  return true;
}

void ScriptRunner::ExecuteTask() {
  // This method is triggered by ScriptRunner::PostTask, and runs directly from
  // the scheduler. So, the call stack is safe to reenter.
  scheduler::CooperativeSchedulingManager::AllowedStackScope
      allowed_stack_scope(scheduler::CooperativeSchedulingManager::Instance());

  if (IsExecutionSuspended())
    return;

  if (ExecuteAsyncTask())
    return;

  if (ExecuteInOrderTask())
    return;
}

void ScriptRunner::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
  visitor->Trace(pending_delayed_async_scripts_);
  visitor->Trace(async_scripts_to_execute_soon_);
  visitor->Trace(in_order_scripts_to_execute_soon_);
}

}  // namespace blink
