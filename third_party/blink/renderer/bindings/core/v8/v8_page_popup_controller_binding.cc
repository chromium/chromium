// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_page_popup_controller_binding.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/context_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/core/page/page_popup_supplement.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

void PagePopupControllerAttributeGetter(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  DOMWindow* impl = V8Window::ToImpl(holder);
  PagePopupController* cpp_value = nullptr;
  if (LocalFrame* frame = ToLocalDOMWindow(impl)->GetFrame())
    cpp_value = PagePopupSupplement::From(*frame).GetPagePopupController();
  V8SetReturnValue(info, ToV8(cpp_value, holder, info.GetIsolate()));
}

void PagePopupControllerAttributeGetterCallback(
    v8::Local<v8::Name>,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  PagePopupControllerAttributeGetter(info);
}

}  // namespace

void V8PagePopupControllerBinding::InstallPagePopupController(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> window_wrapper) {
  Document* document = DynamicTo<Document>(
      ToExecutionContext(window_wrapper->CreationContext()));
  if (!document || !ContextFeatures::PagePopupEnabled(document))
    return;

  window_wrapper
      ->SetAccessor(
          context, V8AtomicString(context->GetIsolate(), "pagePopupController"),
          PagePopupControllerAttributeGetterCallback)
      .ToChecked();
}

}  // namespace blink
