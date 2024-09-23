// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_page_popup_controller_binding.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

void PagePopupControllerAttributeGetter(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  LocalFrame* frame =
      To<LocalDOMWindow>(V8Window::ToWrappableUnsafe(info.GetIsolate(), holder))
          ->GetFrame();
  if (!frame) {
    bindings::V8SetReturnValue(info, nullptr);
    return;
  }
  bindings::V8SetReturnValue(
      info, PagePopupController::From(*frame->GetPage())
                ->ToV8(ScriptState::ForCurrentRealm(info.GetIsolate())));
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
  Document* document =
      ToLocalDOMWindow(window_wrapper->GetCreationContextChecked())->document();
  if (!document) {
    return;
  }

  window_wrapper
      ->SetNativeDataProperty(
          context, V8AtomicString(context->GetIsolate(), "pagePopupController"),
          PagePopupControllerAttributeGetterCallback, nullptr,
          v8::Local<v8::Value>(), v8::ReadOnly)
      .ToChecked();
}

}  // namespace blink
