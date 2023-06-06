/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/frame/window_or_worker_global_scope.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

void WindowOrWorkerGlobalScope::reportError(ScriptState* script_state,
                                            const ScriptValue& e) {
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  V8ScriptRunner::ReportException(script_state->GetIsolate(), e.V8Value());
}

String WindowOrWorkerGlobalScope::btoa(const String& string_to_encode,
                                       ExceptionState& exception_state) {
  if (string_to_encode.IsNull())
    return String();

  if (!string_to_encode.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be encoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }

  return Base64Encode(
      base::as_bytes(base::make_span(string_to_encode.Latin1())));
}

String WindowOrWorkerGlobalScope::atob(const String& encoded_string,
                                       ExceptionState& exception_state) {
  if (encoded_string.IsNull())
    return String();

  if (!encoded_string.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }
  Vector<char> out;
  if (!Base64Decode(encoded_string, out, Base64DecodePolicy::kForgiving)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded is not correctly encoded.");
    return String();
  }

  return String(out.data(), out.size());
}

bool WindowOrWorkerGlobalScope::crossOriginIsolated() {
  return GetExecutionContext()->CrossOriginIsolatedCapability();
}

// See https://github.com/whatwg/html/issues/7912
// static
String WindowOrWorkerGlobalScope::crossOriginEmbedderPolicy() {
  const PolicyContainer* policy_container =
      GetExecutionContext()->GetPolicyContainer();
  CHECK(policy_container);
  switch (policy_container->GetPolicies().cross_origin_embedder_policy.value) {
    case network::mojom::CrossOriginEmbedderPolicyValue::kNone:
      return "unsafe-none";
    case network::mojom::CrossOriginEmbedderPolicyValue::kCredentialless:
      return "credentialless";
    case network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      return "require-corp";
  }
}

ScriptValue WindowOrWorkerGlobalScope::structuredClone(
    ScriptState* script_state,
    const ScriptValue& message,
    const StructuredSerializeOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    return ScriptValue();
  }
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();

  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(isolate, message, options,
                                                transferables, exception_state);

  if (exception_state.HadException()) {
    return ScriptValue();
  }

  DCHECK(serialized_message);

  auto ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException()) {
    return ScriptValue();
  }

  UnpackedSerializedScriptValue* unpacked =
      SerializedScriptValue::Unpack(std::move(serialized_message));
  DCHECK(unpacked);

  SerializedScriptValue::DeserializeOptions deserialize_options;
  deserialize_options.message_ports = MessagePort::EntanglePorts(
      *ExecutionContext::From(script_state), std::move(ports));

  return ScriptValue(isolate,
                     unpacked->Deserialize(isolate, deserialize_options));
}

}  // namespace blink
