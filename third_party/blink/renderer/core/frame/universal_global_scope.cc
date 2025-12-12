// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/universal_global_scope.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

String UniversalGlobalScope::btoa(const String& string_to_encode,
                                  ExceptionState& exception_state) {
  if (string_to_encode.IsNull()) {
    return String();
  }

  if (!string_to_encode.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be encoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }

  return Base64Encode(base::as_byte_span(string_to_encode.Latin1()));
}

String UniversalGlobalScope::atob(const String& encoded_string,
                                  ExceptionState& exception_state) {
  if (encoded_string.IsNull()) {
    return String();
  }

  if (!encoded_string.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }
  Vector<uint8_t> out;
  if (!Base64Decode(encoded_string, out, Base64DecodePolicy::kForgiving)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded is not correctly encoded.");
    return String();
  }

  return String(out);
}

void UniversalGlobalScope::queueMicrotask(V8VoidFunction* callback) {
  GetExecutionContext()->GetAgent()->event_loop()->EnqueueMicrotask(
      BindOnce(&V8VoidFunction::InvokeAndReportException,
               WrapPersistent(callback), nullptr));
}

ScriptValue UniversalGlobalScope::structuredClone(
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
  auto message_ports = MessagePortArray(*MessagePort::EntanglePorts(
      *ExecutionContext::From(script_state), std::move(ports)));
  deserialize_options.message_ports = &message_ports;

  return ScriptValue(isolate,
                     unpacked->Deserialize(isolate, deserialize_options));
}

void UniversalGlobalScope::reportError(ScriptState* script_state,
                                       const ScriptValue& e) {
  if (!script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  V8ScriptRunner::ReportException(script_state->GetIsolate(), e.V8Value());
}

bool UniversalGlobalScope::isSecureContextForBindings(
    ScriptState* script_state) const {
  return ExecutionContext::From(script_state)->IsSecureContext();
}

void UniversalGlobalScope::Trace(Visitor* visitor) const {
  Supplementable<UniversalGlobalScope>::Trace(visitor);
}

}  // namespace blink
