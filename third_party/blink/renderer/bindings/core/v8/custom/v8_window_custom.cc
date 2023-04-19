/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

template <typename CallbackInfo>
static void LocationAttributeGet(const CallbackInfo& info) {
  v8::Local<v8::Object> holder = info.Holder();
  DOMWindow* window = V8Window::ToImpl(holder);
  window->ReportCoopAccess("location");
  Location* location = window->location();
  DCHECK(location);

  // If we have already created a wrapper object in this world, returns it.
  if (DOMDataStore::SetReturnValue(info.GetReturnValue(), location))
    return;

  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Value> wrapper;

  // Note that this check is gated on whether or not |window| is remote, not
  // whether or not |window| is cross-origin. If |window| is local, the
  // |location| property must always return the same wrapper, even if the
  // cross-origin status changes by changing properties like |document.domain|.
  if (IsA<RemoteDOMWindow>(window)) {
    DOMWrapperWorld& world = DOMWrapperWorld::Current(isolate);
    const auto* location_wrapper_type = location->GetWrapperTypeInfo();
    v8::Local<v8::Object> new_wrapper =
        location_wrapper_type->GetV8ClassTemplate(isolate, world)
            .As<v8::FunctionTemplate>()
            ->NewRemoteInstance()
            .ToLocalChecked();

    DCHECK(!DOMDataStore::ContainsWrapper(location, isolate));
    wrapper = V8DOMWrapper::AssociateObjectWithWrapper(
        isolate, location, location_wrapper_type, new_wrapper);
  } else {
    wrapper = ToV8(location, holder, isolate);
  }

  V8SetReturnValue(info, wrapper);
}

void V8Window::LocationAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocationAttributeGet(info);
}

void V8Window::LocationAttributeGetterCustom(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  LocationAttributeGet(info);
}

void V8Window::FrameElementAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = To<LocalDOMWindow>(V8Window::ToImpl(info.Holder()));
  Element* frameElement = impl->frameElement();

  if (!BindingSecurity::ShouldAllowAccessTo(
          CurrentDOMWindow(info.GetIsolate()), frameElement,
          BindingSecurity::ErrorReportOption::kDoNotReport)) {
    V8SetReturnValueNull(info);
    return;
  }

  // The wrapper for an <iframe> should get its prototype from the context of
  // the frame it's in, rather than its own frame.
  // So, use its containing document as the creation context when wrapping.
  v8::Local<v8::Value> creation_context =
      ToV8(frameElement->GetDocument().domWindow(), info.Holder(),
           info.GetIsolate());
  CHECK(!creation_context.IsEmpty());
  v8::Local<v8::Value> wrapper =
      ToV8(frameElement, v8::Local<v8::Object>::Cast(creation_context),
           info.GetIsolate());
  V8SetReturnValue(info, wrapper);
}

void V8Window::OpenerAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DOMWindow* impl = V8Window::ToWrappableUnsafe(info.This());
  impl->ReportCoopAccess("opener");
  if (!impl->GetFrame())
    return;

  // https://html.spec.whatwg.org/C/#dom-opener
  // 7.1.2.1. Navigating related browsing contexts in the DOM
  // The opener attribute's setter must run these steps:
  // step 1. If the given value is null and this Window object's browsing
  //     context is non-null, then set this Window object's browsing context's
  //     disowned to true.
  //
  // Opener can be shadowed if it is in the same domain.
  // Have a special handling of null value to behave
  // like Firefox. See bug http://b/1224887 & http://b/791706.
  if (value->IsNull()) {
    // impl->frame() has to be a non-null LocalFrame.  Otherwise, the
    // same-origin check would have failed.
    DCHECK(impl->GetFrame());
    To<LocalFrame>(impl->GetFrame())->Loader().SetOpener(nullptr);
  }

  // step 2. If the given value is non-null, then return
  //     ? OrdinaryDefineOwnProperty(this Window object, "opener",
  //     { [[Value]]: the given value, [[Writable]]: true,
  //       [[Enumerable]]: true, [[Configurable]]: true }).
  v8::Isolate* isolate = info.GetIsolate();
  v8::PropertyDescriptor desc(value, /*writable=*/true);
  desc.set_enumerable(true);
  desc.set_configurable(true);
  bool result = false;
  if (!info.This()
           ->DefineProperty(isolate->GetCurrentContext(),
                            V8AtomicString(isolate, "opener"), desc)
           .To(&result)) {
    return;
  }
  if (!result) {
    ExceptionState exception_state(
        isolate, ExceptionContext::Context::kAttributeSet, "Window", "opener");
    exception_state.ThrowTypeError("Cannot redefine the property.");
    return;
  }
}

void V8Window::NamedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DOMWindow* window = V8Window::ToImpl(info.Holder());
  if (!window)
    return;

  Frame* frame = window->GetFrame();
  // window is detached from a frame.
  if (!frame)
    return;

  // Note that named access on WindowProxy is allowed in the cross-origin case.
  // 7.4.5 [[GetOwnProperty]] (P), step 6.
  // https://html.spec.whatwg.org/C/#windowproxy-getownproperty
  //
  // 7.3.3 Named access on the Window object
  // The document-tree child browsing context name property set
  // https://html.spec.whatwg.org/C/#document-tree-child-browsing-context-name-property-set
  Frame* child = frame->Tree().ScopedChild(name);
  if (child) {
    window->ReportCoopAccess("named");
    window->RecordWindowProxyAccessMetrics(
        WebFeature::kWindowProxyCrossOriginAccessNamedGetter,
        WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter);
    UseCounter::Count(CurrentExecutionContext(info.GetIsolate()),
                      WebFeature::kNamedAccessOnWindow_ChildBrowsingContext);

    // step 3. Remove each browsing context from childBrowsingContexts whose
    // active document's origin is not same origin with activeDocument's origin
    // and whose browsing context name does not match the name of its browsing
    // context container's name content attribute value.
    if (BindingSecurity::ShouldAllowNamedAccessTo(window, child->DomWindow()) ||
        name == child->Owner()->BrowsingContextContainerName()) {
      bindings::V8SetReturnValue(
          info, child->DomWindow(), window,
          bindings::V8ReturnValue::kMaybeCrossOriginWindow);
      return;
    }

    UseCounter::Count(
        CurrentExecutionContext(info.GetIsolate()),
        WebFeature::
            kNamedAccessOnWindow_ChildBrowsingContext_CrossOriginNameMismatch);
  }

  // This is a cross-origin interceptor. Check that the caller has access to the
  // named results.
  if (!BindingSecurity::ShouldAllowAccessTo(
          CurrentDOMWindow(info.GetIsolate()), window,
          BindingSecurity::ErrorReportOption::kDoNotReport)) {
    // HTML 7.2.3.3 CrossOriginGetOwnPropertyHelper ( O, P )
    // https://html.spec.whatwg.org/C/#crossorigingetownpropertyhelper-(-o,-p-)
    // step 3. If P is "then", @@toStringTag, @@hasInstance, or
    //   @@isConcatSpreadable, then return PropertyDescriptor{ [[Value]]:
    //   undefined, [[Writable]]: false, [[Enumerable]]: false,
    //   [[Configurable]]: true }.
    if (name == "then") {
      bindings::V8SetReturnValue(info, v8::Undefined(info.GetIsolate()));
      return;
    }

    BindingSecurity::FailedAccessCheckFor(
        info.GetIsolate(), window->GetWrapperTypeInfo(), info.Holder());
    return;
  }

  // Search named items in the document.
  auto* doc = DynamicTo<HTMLDocument>(To<LocalFrame>(frame)->GetDocument());
  if (!doc)
    return;

  bool has_named_item = doc->HasNamedItem(name);
  bool has_id_item = doc->HasElementWithId(name);

  if (!has_named_item && !has_id_item)
    return;
  window->ReportCoopAccess("named");
  window->RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessNamedGetter,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter);

  if (!has_named_item && has_id_item &&
      !doc->ContainsMultipleElementsWithId(name)) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedVariableAccessed);
    bindings::V8SetReturnValue(
        info, doc->getElementById(name), window,
        bindings::V8ReturnValue::kMaybeCrossOriginWindow);
    return;
  }

  HTMLCollection* items = doc->WindowNamedItems(name);
  if (!items->IsEmpty()) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedVariableAccessed);

    // TODO(esprehn): Firefox doesn't return an HTMLCollection here if there's
    // multiple with the same name, but Chrome and Safari does. What's the
    // right behavior?
    if (items->HasExactlyOneItem()) {
      bindings::V8SetReturnValue(
          info, items->item(0), window,
          bindings::V8ReturnValue::kMaybeCrossOriginWindow);
      return;
    }
    bindings::V8SetReturnValue(
        info, items, window, bindings::V8ReturnValue::kMaybeCrossOriginWindow);
    return;
  }
}

}  // namespace blink
