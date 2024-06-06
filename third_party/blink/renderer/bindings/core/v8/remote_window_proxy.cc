/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/remote_window_proxy.h"

#include <algorithm>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "v8/include/v8.h"

namespace blink {

RemoteWindowProxy::RemoteWindowProxy(v8::Isolate* isolate,
                                     RemoteFrame& frame,
                                     DOMWrapperWorld* world)
    : WindowProxy(isolate, frame, world) {}

void RemoteWindowProxy::DisposeContext(Lifecycle next_status,
                                       FrameReuseStatus) {
  DCHECK(next_status == Lifecycle::kV8MemoryIsForciblyPurged ||
         next_status == Lifecycle::kGlobalObjectIsDetached ||
         next_status == Lifecycle::kFrameIsDetached ||
         next_status == Lifecycle::kFrameIsDetachedAndV8MemoryIsPurged);

  // If the current lifecycle is kV8MemoryIsForciblyPurged, next status should
  // be either kFrameIsDetachedAndV8MemoryIsPurged, or kGlobalObjectIsDetached.
  // If the former, |global_proxy_| should become weak, and if the latter, the
  // necessary operations are already done so can return here.
  if (lifecycle_ == Lifecycle::kV8MemoryIsForciblyPurged) {
    DCHECK(next_status == Lifecycle::kGlobalObjectIsDetached ||
           next_status == Lifecycle::kFrameIsDetachedAndV8MemoryIsPurged);
    lifecycle_ = next_status;
    return;
  }

  if (lifecycle_ != Lifecycle::kContextIsInitialized)
    return;

  if ((next_status == Lifecycle::kV8MemoryIsForciblyPurged ||
       next_status == Lifecycle::kGlobalObjectIsDetached) &&
      !global_proxy_.IsEmpty()) {
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Object> global = global_proxy_.Get(GetIsolate());
    auto* window = GetFrame()->DomWindow();
    V8DOMWrapper::ClearNativeInfo(GetIsolate(), global,
                                  V8Window::GetWrapperTypeInfo());
    world_->DomDataStore().ClearIfEqualTo(window, global);
#if DCHECK_IS_ON()
    HeapVector<Member<DOMWrapperWorld>> all_worlds;
    DOMWrapperWorld::AllWorldsInIsolate(GetIsolate(), all_worlds);
    for (auto& world : all_worlds) {
      DCHECK(!world->DomDataStore().EqualTo(window, global));
    }

    DidDetachGlobalObject();
#endif  // DCHECK_IS_ON()
  }

  DCHECK_EQ(lifecycle_, Lifecycle::kContextIsInitialized);
  lifecycle_ = next_status;
}

void RemoteWindowProxy::Initialize() {
  TRACE_EVENT2("v8", "RemoteWindowProxy::Initialize", "IsMainFrame",
               GetFrame()->IsMainFrame(), "IsOutermostMainFrame",
               GetFrame()->IsOutermostMainFrame());
  ScriptForbiddenScope::AllowUserAgentScript allow_script;

  v8::HandleScope handle_scope(GetIsolate());
  CreateContext();
  SetupWindowPrototypeChain();
}

void RemoteWindowProxy::CreateContext() {
  // Create a new v8::Context with the window object as the global object
  // (aka the inner global). Reuse the outer global proxy if it already exists.
  v8::Local<v8::ObjectTemplate> global_template =
      V8Window::GetWrapperTypeInfo()
          ->GetV8ClassTemplate(GetIsolate(), *world_)
          .As<v8::FunctionTemplate>()
          ->InstanceTemplate();
  CHECK(!global_template.IsEmpty());

  v8::Local<v8::Object> global_proxy =
      v8::Context::NewRemoteContext(GetIsolate(), global_template,
                                    global_proxy_.Get(GetIsolate()))
          .ToLocalChecked();
  if (global_proxy_.IsEmpty())
    global_proxy_.Reset(GetIsolate(), global_proxy);
  else
    DCHECK(global_proxy_ == global_proxy);
  CHECK(!global_proxy_.IsEmpty());

#if DCHECK_IS_ON()
  DidAttachGlobalObject();
#endif

  DCHECK(lifecycle_ == Lifecycle::kContextIsUninitialized ||
         lifecycle_ == Lifecycle::kGlobalObjectIsDetached);
  lifecycle_ = Lifecycle::kContextIsInitialized;
}

void RemoteWindowProxy::SetupWindowPrototypeChain() {
  // Associate the window wrapper object and its prototype chain with the
  // corresponding native DOMWindow object.
  DOMWindow* window = GetFrame()->DomWindow();
  const WrapperTypeInfo* wrapper_type_info = window->GetWrapperTypeInfo();

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> global_proxy = global_proxy_.Get(GetIsolate());
  V8DOMWrapper::SetNativeInfo(GetIsolate(), global_proxy, window);
  CHECK(global_proxy == window->AssociateWithWrapper(GetIsolate(), world_,
                                                     wrapper_type_info,
                                                     global_proxy));

  // The global object, aka window wrapper object.
  v8::Local<v8::Object> window_wrapper =
      global_proxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::SetNativeInfo(GetIsolate(), window_wrapper, window);
}

}  // namespace blink
