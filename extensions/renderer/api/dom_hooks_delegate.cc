// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/dom_hooks_delegate.h"

#include <memory>

#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_element.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

DOMHooksDelegate::DOMHooksDelegate() = default;
DOMHooksDelegate::~DOMHooksDelegate() = default;

APIBindingHooks::RequestResult DOMHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  using RequestResult = APIBindingHooks::RequestResult;

  v8::Isolate* isolate = context->GetIsolate();
  v8::TryCatch try_catch(isolate);
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    if (try_catch.HasCaught()) {
      try_catch.ReThrow();
      return RequestResult(RequestResult::THROWN);
    }
    return RequestResult(RequestResult::INVALID_INVOCATION);
  }

  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  DCHECK(script_context);

  APIBindingHooks::RequestResult result(
      APIBindingHooks::RequestResult::HANDLED);
  if (method_name == "dom.openOrClosedShadowRoot") {
    DCHECK(parse_result.arguments.has_value());
    result.return_value =
        OpenOrClosedShadowRoot(script_context, *parse_result.arguments);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return result;
}

v8::Local<v8::Value> DOMHooksDelegate::OpenOrClosedShadowRoot(
    ScriptContext* script_context,
    const v8::LocalVector<v8::Value>& parsed_arguments) {
  DCHECK(script_context->extension());
  DCHECK(parsed_arguments[0]->IsObject());

  blink::WebElement element = blink::WebElement::FromV8Value(
      script_context->isolate(), parsed_arguments[0]);
  if (element.IsNull())
    return v8::Null(script_context->isolate());

  blink::WebNode shadow_root = element.OpenOrClosedShadowRoot();
  if (shadow_root.IsNull())
    return v8::Null(script_context->isolate());

  return shadow_root.ToV8Value(script_context->isolate());
}

}  // namespace extensions
