/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

WorkerNavigator::WorkerNavigator(ExecutionContext* execution_context)
    : NavigatorBase(execution_context) {}

WorkerNavigator::~WorkerNavigator() = default;

String WorkerNavigator::GetAcceptLanguages() {
  auto* global_scope = To<WorkerOrWorkletGlobalScope>(GetExecutionContext());
  if (!global_scope) {
    // Prospective fix for crbug.com/40945292 and crbug.com/40827704
    // Return empty string since it is better than crashing here, and the return
    // value is not that important since the worker context is already
    // destroyed.
    return "";
  }

  return global_scope->GetAcceptLanguages();
}

void WorkerNavigator::NotifyUpdate() {
  SetLanguagesDirty();
  WorkerOrWorkletGlobalScope* global_scope =
      To<WorkerOrWorkletGlobalScope>(GetExecutionContext());
  global_scope->DispatchEvent(
      *Event::Create(event_type_names::kLanguagechange));
}

}  // namespace blink
