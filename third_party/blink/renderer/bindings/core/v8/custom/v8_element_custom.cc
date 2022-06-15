// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {

// TODO(https://crbug.com/1335986): Custom setter is needed to collect metrics,
// and can be removed once metrics are captured.
// static
void V8Element::InnerHTMLAttributeSetterCustom(
    v8::Local<v8::Value> html_value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  const ExceptionState::ContextType exception_state_context_type =
      ExceptionContext::Context::kAttributeSet;
  const char* const class_like_name = "Element";
  const char* const property_name = "innerHTML";
  ExceptionState exception_state(isolate, exception_state_context_type,
                                 class_like_name, property_name);

  // [CEReactions]
  CEReactionsScope ce_reactions_scope;

  v8::Local<v8::Object> v8_receiver = info.This();
  Element* blink_receiver = V8Element::ToWrappableUnsafe(v8_receiver);
  ExecutionContext* execution_context_of_document_tree =
      bindings::ExecutionContextFromV8Wrappable(blink_receiver);
  const bool is_high_resolution_timer = base::TimeTicks::IsHighResolution();
  base::ElapsedTimer timer;
  auto&& html = NativeValueTraits<
      IDLStringStringContextTrustedHTMLTreatNullAsEmptyString>::
      NativeValue(isolate, html_value, exception_state,
                  execution_context_of_document_tree);
  if (is_high_resolution_timer) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.SetInnerHtml.ConversionTime", timer.Elapsed(),
        base::Microseconds(1), base::Seconds(1), 100);
  }
  if (UNLIKELY(exception_state.HadException())) {
    return;
  }
  timer = base::ElapsedTimer();
  blink_receiver->setInnerHTML(html, exception_state);
  if (is_high_resolution_timer) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.SetInnerHtml.ExecutionTime", timer.Elapsed(),
        base::Microseconds(1), base::Seconds(10), 100);
    UMA_HISTOGRAM_COUNTS_1M("Blink.SetInnerHtml.StringLength", html.length());
  }
  if (UNLIKELY(exception_state.HadException())) {
    return;
  }
}

}  // namespace blink
