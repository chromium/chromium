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
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

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
      break;

    case ScriptSchedulingType::kInOrder:
      pending_in_order_scripts_.push_back(pending_script);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ScriptRunner::NotifyScriptReady(PendingScript* pending_script) {
  switch (pending_script->GetSchedulingType()) {
    case ScriptSchedulingType::kAsync:
      CHECK(pending_async_scripts_.Contains(pending_script));
      pending_async_scripts_.erase(pending_script);

      task_runner_->PostTask(
          FROM_HERE,
          WTF::Bind(&ScriptRunner::ExecutePendingScript,
                    WrapWeakPersistent(this), WrapPersistent(pending_script)));
      break;

    case ScriptSchedulingType::kInOrder:
      while (!pending_in_order_scripts_.IsEmpty() &&
             pending_in_order_scripts_.front()->IsReady()) {
        PendingScript* pending_in_order = pending_in_order_scripts_.TakeFirst();
        task_runner_->PostTask(FROM_HERE,
                               WTF::Bind(&ScriptRunner::ExecutePendingScript,
                                         WrapWeakPersistent(this),
                                         WrapPersistent(pending_in_order)));
      }
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

void ScriptRunner::ExecutePendingScript(PendingScript* pending_script) {
  TRACE_EVENT("blink", "ScriptRunner::ExecutePendingScript");

  DCHECK(!document_->domWindow() || !document_->domWindow()->IsContextPaused());
  DCHECK(pending_script);

  pending_script->ExecuteScriptBlock(NullURL());

  document_->DecrementLoadEventDelayCount();
}

void ScriptRunner::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
}

}  // namespace blink
