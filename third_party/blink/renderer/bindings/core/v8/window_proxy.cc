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

#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "v8/include/v8.h"

namespace blink {

WindowProxy::~WindowProxy() {
  // clearForClose() or clearForNavigation() must be invoked before destruction
  // starts.
  DCHECK(lifecycle_ != Lifecycle::kContextIsInitialized);
}

void WindowProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
}

WindowProxy::WindowProxy(v8::Isolate* isolate,
                         Frame& frame,
                         scoped_refptr<DOMWrapperWorld> world)
    : isolate_(isolate),
      frame_(frame),

      world_(std::move(world)),
      lifecycle_(Lifecycle::kContextIsUninitialized) {}

void WindowProxy::ClearForClose() {
  DisposeContext(lifecycle_ == Lifecycle::kV8MemoryIsForciblyPurged
                     ? Lifecycle::kFrameIsDetachedAndV8MemoryIsPurged
                     : Lifecycle::kFrameIsDetached,
                 kFrameWillNotBeReused,
                 v8::Context::DetachedWindowReason::kDetachedWindowByClosing);
}

void WindowProxy::ClearForNavigation() {
  DisposeContext(Lifecycle::kGlobalObjectIsDetached, kFrameWillBeReused,
                 v8::Context::kDetachedWindowByNavigation);
}

void WindowProxy::ClearForSwap() {
  // This happens on a navigation between local/remote source.
  DisposeContext(Lifecycle::kGlobalObjectIsDetached, kFrameWillNotBeReused,
                 v8::Context::kDetachedWindowByNavigation);
}

void WindowProxy::ClearForV8MemoryPurge() {
  DisposeContext(Lifecycle::kV8MemoryIsForciblyPurged, kFrameWillNotBeReused,
                 v8::Context::kDetachedWindowByOtherReason);
}

v8::Local<v8::Object> WindowProxy::GlobalProxyIfNotDetached() {
  if (lifecycle_ == Lifecycle::kContextIsInitialized) {
    DLOG_IF(FATAL, !is_global_object_attached_)
        << "Context is initialized but global object is detached!";
    return global_proxy_.NewLocal(isolate_);
  }
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WindowProxy::ReleaseGlobalProxy() {
  DCHECK(lifecycle_ == Lifecycle::kContextIsUninitialized ||
         lifecycle_ == Lifecycle::kGlobalObjectIsDetached);

  // Make sure the global object was detached from the proxy by calling
  // clearForNavigation().
  DLOG_IF(FATAL, is_global_object_attached_)
      << "Context not detached by calling clearForNavigation()";

  v8::Local<v8::Object> global_proxy = global_proxy_.NewLocal(isolate_);
  global_proxy_.Clear();
  return global_proxy;
}

void WindowProxy::SetGlobalProxy(v8::Local<v8::Object> global_proxy) {
  DCHECK_EQ(lifecycle_, Lifecycle::kContextIsUninitialized);

  CHECK(global_proxy_.IsEmpty());
  global_proxy_.Set(isolate_, global_proxy);

  // Initialize the window proxy now, to re-establish the connection between
  // the global object and the v8::Context. This is really only needed for a
  // RemoteDOMWindow, since it has no scripting environment of its own.
  // Without this, existing script references to a swapped in RemoteDOMWindow
  // would be broken until that RemoteDOMWindow was vended again through an
  // interface like window.frames.
  Initialize();
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a DOMWindow instance. However, to
// allow properties of the JS DOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS DOMWindow
// instance as the prototype for that shadow object. The JS DOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the DOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// DOMWindow instance is the shadow object. When mapping a DOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a LocalFrame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
void WindowProxy::InitializeIfNeeded() {
  if (lifecycle_ == Lifecycle::kContextIsUninitialized ||
      lifecycle_ == Lifecycle::kGlobalObjectIsDetached) {
    Initialize();
  }
}

v8::Local<v8::Object> WindowProxy::AssociateWithWrapper(
    DOMWindow* window,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Object> wrapper) {
  if (world_->DomDataStore().Set(isolate_, window, wrapper_type_info,
                                 wrapper)) {
    WrapperTypeInfo::WrapperCreated();
    V8DOMWrapper::SetNativeInfo(isolate_, wrapper, wrapper_type_info, window);
    DCHECK(V8DOMWrapper::HasInternalFieldsSet(wrapper));
  }
  SECURITY_CHECK(ToScriptWrappable(wrapper) == window);
  return wrapper;
}

}  // namespace blink
