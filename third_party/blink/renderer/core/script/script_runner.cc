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
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

ScriptRunner::ScriptRunner(Document* document)
    : document_(document),
      task_runner_(document->GetTaskRunner(TaskType::kNetworking)) {
  DCHECK(document);
}

void ScriptRunner::QueueScriptForExecution(PendingScript* pending_script) {
  DCHECK(pending_script);
  document_->IncrementLoadEventDelayCount();
  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      pending_async_scripts_.insert(pending_script);
      TryStream(pending_script);
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

void ScriptRunner::Suspend() {
#ifndef NDEBUG
  // Resume will re-post tasks for all available scripts.
  number_of_extra_tasks_ += async_scripts_to_execute_soon_.size() +
                            in_order_scripts_to_execute_soon_.size();
#endif

  is_suspended_ = true;
}

void ScriptRunner::Resume() {
  DCHECK(is_suspended_);

  is_suspended_ = false;

  for (size_t i = 0; i < async_scripts_to_execute_soon_.size(); ++i) {
    PostTask(FROM_HERE);
  }
  for (size_t i = 0; i < in_order_scripts_to_execute_soon_.size(); ++i) {
    PostTask(FROM_HERE);
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

void ScriptRunner::NotifyScriptReady(PendingScript* pending_script) {
  SECURITY_CHECK(pending_script);
  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      // SECURITY_CHECK() makes us crash in a controlled way in error cases
      // where the PendingScript is associated with the wrong ScriptRunner
      // (otherwise we'd cause a use-after-free in ~ScriptRunner when it tries
      // to detach).
      SECURITY_CHECK(pending_async_scripts_.Contains(pending_script));

      pending_async_scripts_.erase(pending_script);
      async_scripts_to_execute_soon_.push_back(pending_script);

      PostTask(FROM_HERE);
      TryStreamAny();
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

void ScriptRunner::NotifyScriptStreamerFinished() {
  PostTask(FROM_HERE);
  TryStreamAny();
}

void ScriptRunner::MovePendingScript(Document& old_document,
                                     Document& new_document,
                                     ScriptLoader* script_loader) {
  Document* new_context_document = new_document.ContextDocument();
  if (!new_context_document) {
    // Document's contextDocument() method will return no Document if the
    // following conditions both hold:
    //
    //   - The Document wasn't created with an explicit context document
    //     and that document is otherwise kept alive.
    //   - The Document itself is detached from its frame.
    //
    // The script element's loader is in that case moved to document() and
    // its script runner, which is the non-null Document that contextDocument()
    // would return if not detached.
    DCHECK(!new_document.GetFrame());
    new_context_document = &new_document;
  }
  Document* old_context_document = old_document.ContextDocument();
  if (!old_context_document) {
    DCHECK(!old_document.GetFrame());
    old_context_document = &old_document;
  }

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
  // Find an async script loader which is not currently streaming.
  auto it = std::find_if(async_scripts_to_execute_soon_.begin(),
                         async_scripts_to_execute_soon_.end(),
                         [](PendingScript* pending_script) {
                           DCHECK(pending_script);
                           return !pending_script->IsCurrentlyStreaming();
                         });
  if (it == async_scripts_to_execute_soon_.end()) {
    return false;
  }

  // Remove the async script loader from the ready-to-exec set and execute.
  PendingScript* pending_script = *it;
  async_scripts_to_execute_soon_.erase(it);

  DCHECK_EQ(pending_script->GetSchedulingType(), ScriptSchedulingType::kAsync)
      << "Async scripts queue should not contain any in-order script.";

  pending_script->ExecuteScriptBlock(NullURL());

  document_->DecrementLoadEventDelayCount();
  return true;
}

void ScriptRunner::ExecuteTask() {
  if (is_suspended_)
    return;

  if (ExecuteAsyncTask())
    return;

  if (ExecuteInOrderTask())
    return;

#ifndef NDEBUG
  // Extra tasks should be posted only when we resume after suspending,
  // or when we stream a script. These should all be accounted for in
  // number_of_extra_tasks_.
  DCHECK_GT(number_of_extra_tasks_--, 0);
#endif
}

void ScriptRunner::TryStreamAny() {
  if (is_suspended_)
    return;

  if (!RuntimeEnabledFeatures::WorkStealingInScriptRunnerEnabled())
    return;

  // Look through async_scripts_to_execute_soon_, and stream any one of them.
  for (auto pending_script : async_scripts_to_execute_soon_) {
    if (DoTryStream(pending_script))
      return;
  }
}

void ScriptRunner::TryStream(PendingScript* pending_script) {
  if (!is_suspended_)
    DoTryStream(pending_script);
}

bool ScriptRunner::DoTryStream(PendingScript* pending_script) {
  // Checks that all callers should have already done.
  DCHECK(!is_suspended_);
  DCHECK(pending_script);

  // Currently, we stream only async scripts in this function.
  // Note: HTMLParserScriptRunner kicks streaming for deferred or blocking
  // scripts.
  DCHECK(pending_async_scripts_.find(pending_script) !=
             pending_async_scripts_.end() ||
         std::find(async_scripts_to_execute_soon_.begin(),
                   async_scripts_to_execute_soon_.end(),
                   pending_script) != async_scripts_to_execute_soon_.end());

  if (!pending_script)
    return false;

  DCHECK_EQ(pending_script->GetSchedulingType(), ScriptSchedulingType::kAsync);

#ifndef NDEBUG
  bool was_already_streaming = pending_script->IsCurrentlyStreaming();
#endif

  bool success = pending_script->StartStreamingIfPossible(
      WTF::Bind(&ScriptRunner::NotifyScriptStreamerFinished,
                WrapWeakPersistent(this)));
#ifndef NDEBUG
  if (was_already_streaming) {
    DCHECK(!success);
  } else {
    DCHECK_EQ(success, pending_script->IsCurrentlyStreaming());
  }
  if (success)
    number_of_extra_tasks_++;
#endif
  return success;
}

void ScriptRunner::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
  visitor->Trace(async_scripts_to_execute_soon_);
  visitor->Trace(in_order_scripts_to_execute_soon_);
}

}  // namespace blink
