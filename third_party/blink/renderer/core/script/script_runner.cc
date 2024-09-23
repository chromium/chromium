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
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RaceTaskPriority {
  kLowerPriority = 0,
  kNormalPriority = 1,
  kMaxValue = kNormalPriority,
};

const char* RaceTaskPriorityToString(RaceTaskPriority task_priority) {
  switch (task_priority) {
    case RaceTaskPriority::kLowerPriority:
      return "LowerPriority";
    case RaceTaskPriority::kNormalPriority:
      return "NormalPriority";
  }
}

void PostTaskWithLowPriorityUntilTimeout(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta timeout,
    scoped_refptr<base::SingleThreadTaskRunner> lower_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> normal_priority_task_runner) {
  using RefCountedOnceClosure = base::RefCountedData<base::OnceClosure>;
  scoped_refptr<RefCountedOnceClosure> ref_counted_task =
      base::MakeRefCounted<RefCountedOnceClosure>(std::move(task));

  // |run_task_once| runs on both of |lower_priority_task_runner| and
  // |normal_priority_task_runner|. |run_task_once| guarantees that the given
  // |task| doesn't run more than once. |task| runs on either of
  // |lower_priority_task_runner| and |normal_priority_task_runner| whichever
  // comes first.
  auto run_task_once = [](scoped_refptr<RefCountedOnceClosure> ref_counted_task,
                          RaceTaskPriority task_priority,
                          base::TimeTicks post_task_time) {
    if (!ref_counted_task->data.is_null()) {
      auto duration = base::TimeTicks::Now() - post_task_time;
      std::move(ref_counted_task->data).Run();
      base::UmaHistogramEnumeration(
          "Blink.Script.PostTaskWithLowPriorityUntilTimeout.RaceTaskPriority",
          task_priority);
      base::UmaHistogramMediumTimes(
          "Blink.Script.PostTaskWithLowPriorityUntilTimeout.Time", duration);
      base::UmaHistogramMediumTimes(
          base::StrCat(
              {"Blink.Script.PostTaskWithLowPriorityUntilTimeout.Time.",
               RaceTaskPriorityToString(task_priority)}),
          duration);
    }
  };

  base::TimeTicks post_task_time = base::TimeTicks::Now();

  lower_priority_task_runner->PostTask(
      from_here,
      WTF::BindOnce(run_task_once, ref_counted_task,
                    RaceTaskPriority::kLowerPriority, post_task_time));

  normal_priority_task_runner->PostDelayedTask(
      from_here,
      WTF::BindOnce(run_task_once, ref_counted_task,
                    RaceTaskPriority::kNormalPriority, post_task_time),
      timeout);
}

}  // namespace

namespace blink {

void PostTaskWithLowPriorityUntilTimeoutForTesting(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta timeout,
    scoped_refptr<base::SingleThreadTaskRunner> lower_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> normal_priority_task_runner) {
  PostTaskWithLowPriorityUntilTimeout(from_here, std::move(task), timeout,
                                      std::move(lower_priority_task_runner),
                                      std::move(normal_priority_task_runner));
}

ScriptRunner::ScriptRunner(Document* document)
    : document_(document),
      task_runner_(document->GetTaskRunner(TaskType::kNetworking)),
      low_priority_task_runner_(
          document->GetTaskRunner(TaskType::kLowPriorityScriptExecution)) {
  DCHECK(document);
}

void ScriptRunner::QueueScriptForExecution(PendingScript* pending_script,
                                           DelayReasons delay_reasons) {
  DCHECK(pending_script);
  DCHECK(delay_reasons & static_cast<DelayReasons>(DelayReason::kLoad));
  document_->IncrementLoadEventDelayCount();

  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      pending_async_scripts_.insert(pending_script, delay_reasons);
      break;

    case ScriptSchedulingType::kInOrder:
      pending_in_order_scripts_.push_back(pending_script);
      break;

    case ScriptSchedulingType::kForceInOrder:
      pending_force_in_order_scripts_.push_back(pending_script);
      pending_force_in_order_scripts_count_ += 1;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Note that WatchForLoad() can immediately call PendingScriptFinished().
  pending_script->WatchForLoad(this);
}

void ScriptRunner::AddDelayReason(DelayReason delay_reason) {
  DCHECK(!IsActive(delay_reason));
  active_delay_reasons_ |= static_cast<DelayReasons>(delay_reason);
}

void ScriptRunner::RemoveDelayReason(DelayReason delay_reason) {
  DCHECK(IsActive(delay_reason));
  active_delay_reasons_ &= ~static_cast<DelayReasons>(delay_reason);

  HeapVector<Member<PendingScript>> pending_async_scripts;
  CopyKeysToVector(pending_async_scripts_, pending_async_scripts);
  for (PendingScript* pending_script : pending_async_scripts) {
    RemoveDelayReasonFromScript(pending_script, delay_reason);
  }
}

void ScriptRunner::RemoveDelayReasonFromScript(PendingScript* pending_script,
                                               DelayReason delay_reason) {
  // |pending_script| can be null when |RemoveDelayReasonFromScript()| is called
  // via |PostDelayedTask()| below.
  if (!pending_script)
    return;

  auto it = pending_async_scripts_.find(pending_script);

  if (it == pending_async_scripts_.end())
    return;

  if (it->value &= ~static_cast<DelayReasons>(delay_reason)) {
    // The delay must be less than a few seconds because some scripts times out
    // otherwise. This is only applied to milestone based delay.
    const base::TimeDelta delay_limit =
        features::kDelayAsyncScriptExecutionDelayLimitParam.Get();
    if (!delay_limit.is_zero() && delay_reason == DelayReason::kLoad &&
        (it->value & static_cast<DelayReasons>(DelayReason::kMilestone))) {
      // PostDelayedTask to limit the delay amount of DelayAsyncScriptExecution
      // (see crbug/1340837). DelayReason::kMilestone is sent on
      // loading-milestones such as LCP, first_paint, or finished_parsing.
      // Once the script is completely loaded, even if the milestones delaying
      // execution aren't removed, we eventually want to trigger
      // script-execution anyway for compatibility reasons, since waiting too
      // long for the milestones can cause compatibility issues.
      // |pending_script| has to be wrapped by WrapWeakPersistent because the
      // following delayed task should not persist a PendingScript.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          WTF::BindOnce(&ScriptRunner::RemoveDelayReasonFromScript,
                        WrapWeakPersistent(this),
                        WrapWeakPersistent(pending_script),
                        DelayReason::kMilestone),
          delay_limit);
    }
    // Still to be delayed.
    return;
  }

  // Script is really ready to evaluate.
  pending_async_scripts_.erase(it);
  base::OnceClosure task = WTF::BindOnce(
      &ScriptRunner::ExecuteAsyncPendingScript, WrapWeakPersistent(this),
      WrapPersistent(pending_script), base::TimeTicks::Now());
  if (pending_script->IsEligibleForLowPriorityAsyncScriptExecution()) {
    PostTaskWithLowPriorityUntilTimeout(
        FROM_HERE, std::move(task),
        features::kTimeoutForLowPriorityAsyncScriptExecution.Get(),
        low_priority_task_runner_, task_runner_);
  } else {
    task_runner_->PostTask(FROM_HERE, std::move(task));
  }
}

void ScriptRunner::ExecuteAsyncPendingScript(
    PendingScript* pending_script,
    base::TimeTicks ready_to_evaluate_time) {
  base::UmaHistogramMediumTimes(
      "Blink.Script.AsyncScript.FromReadyToStartExecution.Time",
      base::TimeTicks::Now() - ready_to_evaluate_time);
  ExecutePendingScript(pending_script);
}

void ScriptRunner::ExecuteForceInOrderPendingScript(
    PendingScript* pending_script) {
  DCHECK_GT(pending_force_in_order_scripts_count_, 0u);
  ExecutePendingScript(pending_script);
  pending_force_in_order_scripts_count_ -= 1;
}

void ScriptRunner::ExecuteParserBlockingScriptsBlockedByForceInOrder() {
  ScriptableDocumentParser* parser = document_->GetScriptableDocumentParser();
  if (parser && document_->IsScriptExecutionReady()) {
    parser->ExecuteScriptsWaitingForResources();
  }
}

void ScriptRunner::PendingScriptFinished(PendingScript* pending_script) {
  pending_script->StopWatchingForLoad();

  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      CHECK(pending_async_scripts_.Contains(pending_script));
      RemoveDelayReasonFromScript(pending_script, DelayReason::kLoad);
      break;

    case ScriptSchedulingType::kInOrder:
      while (!pending_in_order_scripts_.empty() &&
             pending_in_order_scripts_.front()->IsReady()) {
        PendingScript* pending_in_order = pending_in_order_scripts_.TakeFirst();
        task_runner_->PostTask(
            FROM_HERE, WTF::BindOnce(&ScriptRunner::ExecutePendingScript,
                                     WrapWeakPersistent(this),
                                     WrapPersistent(pending_in_order)));
      }
      break;

    case ScriptSchedulingType::kForceInOrder:
      while (!pending_force_in_order_scripts_.empty() &&
             pending_force_in_order_scripts_.front()->IsReady()) {
        PendingScript* pending_in_order =
            pending_force_in_order_scripts_.TakeFirst();
        task_runner_->PostTask(
            FROM_HERE,
            WTF::BindOnce(&ScriptRunner::ExecuteForceInOrderPendingScript,
                          WrapWeakPersistent(this),
                          WrapPersistent(pending_in_order)));
      }
      if (pending_force_in_order_scripts_.empty()) {
        task_runner_->PostTask(
            FROM_HERE,
            WTF::BindOnce(&ScriptRunner::
                              ExecuteParserBlockingScriptsBlockedByForceInOrder,
                          WrapWeakPersistent(this)));
      }
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ScriptRunner::ExecutePendingScript(PendingScript* pending_script) {
  TRACE_EVENT("blink", "ScriptRunner::ExecutePendingScript");

  DCHECK(!document_->domWindow() || !document_->domWindow()->IsContextPaused());
  DCHECK(pending_script);

  pending_script->ExecuteScriptBlock();

  document_->DecrementLoadEventDelayCount();
}

void ScriptRunner::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
  visitor->Trace(pending_force_in_order_scripts_);
  PendingScriptClient::Trace(visitor);
}

ScriptRunnerDelayer::ScriptRunnerDelayer(ScriptRunner* script_runner,
                                         ScriptRunner::DelayReason delay_reason)
    : script_runner_(script_runner), delay_reason_(delay_reason) {}

void ScriptRunnerDelayer::Activate() {
  if (activated_)
    return;
  activated_ = true;
  if (script_runner_)
    script_runner_->AddDelayReason(delay_reason_);
}

void ScriptRunnerDelayer::Deactivate() {
  if (!activated_)
    return;
  activated_ = false;
  if (script_runner_)
    script_runner_->RemoveDelayReason(delay_reason_);
}

void ScriptRunnerDelayer::Trace(Visitor* visitor) const {
  visitor->Trace(script_runner_);
}

}  // namespace blink
