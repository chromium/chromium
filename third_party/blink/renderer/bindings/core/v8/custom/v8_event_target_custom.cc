/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

void V8EventTarget::AddEventListenerMethodPrologueCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    EventTarget*) {
  if (info.Length() >= 3 && info[2]->IsObject()) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kAddEventListenerThirdArgumentIsObject);
  }
  if (info.Length() >= 4) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kAddEventListenerFourArguments);
  }
}

void V8EventTarget::RemoveEventListenerMethodPrologueCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    EventTarget*) {
  if (info.Length() >= 3 && info[2]->IsObject()) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kRemoveEventListenerThirdArgumentIsObject);
  }
  if (info.Length() >= 4) {
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kRemoveEventListenerFourArguments);
  }
}

}  // namespace blink
