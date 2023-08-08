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

void V8Window::NamedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DOMWindow* window = V8Window::ToWrappableUnsafe(info.Holder());
  if (!window)
    return;

  Frame* frame = window->GetFrame();
  // window is detached from a frame.
  if (!frame)
    return;

  // Verify that COOP: restrict-properties does not prevent this access.
  // TODO(https://crbug.com/1467216): This will block all same-origin only
  // properties accesses with a "Named property" access failure, because the
  // properties will be tried here as part of the algorithm. See if we need to
  // have a custom message in that case, possibly by actually printing the
  // passed name.
  if (UNLIKELY(
          window->IsAccessBlockedByCoopRestrictProperties(info.GetIsolate()))) {
    // We need to not throw an exception if we're dealing with the special
    // "then" property but return undefined instead. See
    // https://html.spec.whatwg.org/#crossoriginpropertyfallback-(-p-). This
    // makes sure WindowProxy is thenable, see the original discussion here:
    // https://github.com/whatwg/dom/issues/536.
    if (name == "then") {
      return info.GetReturnValue().SetUndefined();
    }

    ExceptionState exception_state(info.GetIsolate(),
                                   ExceptionState::kNamedGetterContext,
                                   "Window", name.Utf8().c_str());
    exception_state.ThrowSecurityError(
        "Cross-Origin-Opener-Policy: 'restrict-properties' blocked the access.",
        "Cross-Origin-Opener-Policy: 'restrict-properties' blocked the "
        "access.");
    return info.GetReturnValue().SetNull();
  }

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
    if (frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            child->GetSecurityContext()->GetSecurityOrigin()) ||
        name == child->Owner()->BrowsingContextContainerName()) {
      bindings::V8SetReturnValue(info, child->DomWindow(), window,
                                 bindings::V8ReturnValue::kMaybeCrossOrigin);
      return;
    }

    UseCounter::Count(
        CurrentExecutionContext(info.GetIsolate()),
        WebFeature::
            kNamedAccessOnWindow_ChildBrowsingContext_CrossOriginNameMismatch);
  }

  // This is a cross-origin interceptor. Check that the caller has access to the
  // named results.
  if (!BindingSecurity::ShouldAllowAccessTo(CurrentDOMWindow(info.GetIsolate()),
                                            window)) {
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
    bindings::V8SetReturnValue(info, doc->getElementById(name), window,
                               bindings::V8ReturnValue::kMaybeCrossOrigin);
    return;
  }

  HTMLCollection* items = doc->WindowNamedItems(name);
  if (!items->IsEmpty()) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedVariableAccessed);

    // TODO(esprehn): Firefox doesn't return an HTMLCollection here if there's
    // multiple with the same name, but Chrome and Safari does. What's the
    // right behavior?
    if (items->HasExactlyOneItem()) {
      bindings::V8SetReturnValue(info, items->item(0), window,
                                 bindings::V8ReturnValue::kMaybeCrossOrigin);
      return;
    }
    bindings::V8SetReturnValue(info, items, window,
                               bindings::V8ReturnValue::kMaybeCrossOrigin);
    return;
  }
}

}  // namespace blink
