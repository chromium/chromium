// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/window_properties.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"

namespace blink {

v8::Local<v8::Value> WindowProperties::AnonymousNamedGetter(
    const AtomicString& name) {
  DOMWindow* window = static_cast<DOMWindow*>(this);
  Frame* frame = window->GetFrame();
  if (!frame) {
    return v8::Local<v8::Value>();
  }

  v8::Isolate* isolate = frame->GetWindowProxyManager()->GetIsolate();

  if (auto reason = window->GetProxyAccessBlockedReason(isolate)) [[unlikely]] {
    // We need to not throw an exception if we're dealing with the special
    // "then" property but return undefined instead. See
    // https://html.spec.whatwg.org/#crossoriginpropertyfallback-(-p-). This
    // makes sure WindowProxy is thenable, see the original discussion here:
    // https://github.com/whatwg/dom/issues/536.
    if (name == "then") {
      return v8::Local<v8::Value>();
    }
    V8ThrowDOMException::Throw(
        isolate, DOMExceptionCode::kSecurityError,
        DOMWindow::GetProxyAccessBlockedExceptionMessage(*reason));
    return v8::Null(isolate);
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
        WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter,
        mojom::blink::WindowProxyAccessType::kAnonymousNamedGetter);
    UseCounter::Count(CurrentExecutionContext(isolate),
                      WebFeature::kNamedAccessOnWindow_ChildBrowsingContext);

    // step 3. Remove each browsing context from childBrowsingContexts whose
    // active document's origin is not same origin with activeDocument's origin
    // and whose browsing context name does not match the name of its browsing
    // context container's name content attribute value.
    if (frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            child->GetSecurityContext()->GetSecurityOrigin()) ||
        name == child->Owner()->BrowsingContextContainerName()) {
      return ToV8Traits<DOMWindow>::ToV8(ScriptState::ForCurrentRealm(isolate),
                                         child->DomWindow());
    }

    UseCounter::Count(
        CurrentExecutionContext(isolate),
        WebFeature::
            kNamedAccessOnWindow_ChildBrowsingContext_CrossOriginNameMismatch);
  }

  // This is a cross-origin interceptor. Check that the caller has access to the
  // named results.
  if (!BindingSecurity::ShouldAllowAccessTo(
          blink::ToLocalDOMWindow(isolate->GetCurrentContext()), window)) {
    return v8::Local<v8::Value>();
  }

  // Search named items in the document.
  auto* doc = DynamicTo<HTMLDocument>(To<LocalDOMWindow>(window)->document());
  if (!doc) {
    return v8::Local<v8::Value>();
  }

  bool has_named_item = doc->HasNamedItem(name);
  bool has_id_item = doc->HasElementWithId(name);

  if (!has_named_item && !has_id_item) {
    return v8::Local<v8::Value>();
  }
  window->ReportCoopAccess("named");
  window->RecordWindowProxyAccessMetrics(
      WebFeature::kWindowProxyCrossOriginAccessNamedGetter,
      WebFeature::kWindowProxyCrossOriginAccessFromOtherPageNamedGetter,
      mojom::blink::WindowProxyAccessType::kAnonymousNamedGetter);

  // If we've reached this point, we know that we're accessing an element (or
  // collection of elements) in this window, and that this window is local. Wrap
  // the return value in this window's relevant context, with the current
  // wrapper world.
  ScriptState* script_state = ToScriptState(To<LocalDOMWindow>(window),
                                            DOMWrapperWorld::Current(isolate));
  if (!has_named_item && has_id_item &&
      !doc->ContainsMultipleElementsWithId(name)) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedWindowPropertyAccessed);
    return ToV8Traits<Element>::ToV8(script_state, doc->getElementById(name));
  }

  HTMLCollection* items = doc->WindowNamedItems(name);
  if (!items->IsEmpty()) {
    UseCounter::Count(doc, WebFeature::kDOMClobberedWindowPropertyAccessed);

    // TODO(esprehn): Firefox doesn't return an HTMLCollection here if there's
    // multiple with the same name, but Chrome and Safari does. What's the
    // right behavior?
    if (items->HasExactlyOneItem()) {
      return ToV8Traits<Element>::ToV8(script_state, items->item(0));
    }
    return ToV8Traits<HTMLCollection>::ToV8(script_state, items);
  }
  return v8::Local<v8::Value>();
}

}  // namespace blink
