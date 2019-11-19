/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/crypto/normalize_algorithm.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

static void RejectWithTypeError(const String& error_details,
                                ScriptPromiseResolver* resolver) {
  // Duplicate some of the checks done by ScriptPromiseResolver.
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  ScriptState::Scope scope(resolver->GetScriptState());
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  resolver->Reject(V8ThrowException::CreateTypeError(isolate, error_details));
}

class CryptoResultImpl::Resolver final : public ScriptPromiseResolver {
 public:
  static Resolver* Create(ScriptState* script_state, CryptoResultImpl* result) {
    DCHECK(script_state->ContextIsValid());
    Resolver* resolver = MakeGarbageCollected<Resolver>(script_state, result);
    resolver->KeepAliveWhilePending();
    return resolver;
  }

  Resolver(ScriptState* script_state, CryptoResultImpl* result)
      : ScriptPromiseResolver(script_state), result_(result) {}

  void ContextDestroyed(ExecutionContext* destroyed_context) override {
    result_->Cancel();
    result_ = nullptr;
    ScriptPromiseResolver::ContextDestroyed(destroyed_context);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(result_);
    ScriptPromiseResolver::Trace(visitor);
  }

 private:
  Member<CryptoResultImpl> result_;
};

ExceptionCode WebCryptoErrorToExceptionCode(WebCryptoErrorType error_type) {
  switch (error_type) {
    case kWebCryptoErrorTypeNotSupported:
      return ToExceptionCode(DOMExceptionCode::kNotSupportedError);
    case kWebCryptoErrorTypeSyntax:
      return ToExceptionCode(DOMExceptionCode::kSyntaxError);
    case kWebCryptoErrorTypeInvalidAccess:
      return ToExceptionCode(DOMExceptionCode::kInvalidAccessError);
    case kWebCryptoErrorTypeData:
      return ToExceptionCode(DOMExceptionCode::kDataError);
    case kWebCryptoErrorTypeOperation:
      return ToExceptionCode(DOMExceptionCode::kOperationError);
    case kWebCryptoErrorTypeType:
      return ToExceptionCode(ESErrorType::kTypeError);
  }

  NOTREACHED();
  return 0;
}

CryptoResultImpl::CryptoResultImpl(ScriptState* script_state)
    : resolver_(Resolver::Create(script_state, this)),
      cancel_(base::MakeRefCounted<CryptoResultCancel>()) {
  // Sync cancellation state.
  if (ExecutionContext::From(script_state)->IsContextDestroyed())
    cancel_->Cancel();
}

CryptoResultImpl::~CryptoResultImpl() {
  DCHECK(!resolver_);
}

void CryptoResultImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
  CryptoResult::Trace(visitor);
}

void CryptoResultImpl::ClearResolver() {
  resolver_ = nullptr;
}

void CryptoResultImpl::CompleteWithError(WebCryptoErrorType error_type,
                                         const WebString& error_details) {
  if (!resolver_)
    return;

  ExceptionCode exception_code = WebCryptoErrorToExceptionCode(error_type);

  // Handle TypeError separately, as it cannot be created using
  // DOMException.
  if (exception_code == ToExceptionCode(ESErrorType::kTypeError)) {
    RejectWithTypeError(error_details, resolver_);
  } else if (IsDOMExceptionCode(exception_code)) {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        static_cast<DOMExceptionCode>(exception_code), error_details));
  } else {
    NOTREACHED();
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, error_details));
  }
  ClearResolver();
}

void CryptoResultImpl::CompleteWithBuffer(const void* bytes,
                                          unsigned bytes_size) {
  if (!resolver_)
    return;

  resolver_->Resolve(DOMArrayBuffer::Create(bytes, bytes_size));
  ClearResolver();
}

void CryptoResultImpl::CompleteWithJson(const char* utf8_data,
                                        unsigned length) {
  if (!resolver_)
    return;

  ScriptState* script_state = resolver_->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);

  // Crashes if longer than v8::String::kMaxLength.
  v8::Local<v8::String> json_string =
      v8::String::NewFromUtf8(isolate, utf8_data, v8::NewStringType::kNormal,
                              length)
          .ToLocalChecked();

  v8::TryCatch exception_catcher(isolate);
  v8::Local<v8::Value> json_dictionary;
  if (v8::JSON::Parse(script_state->GetContext(), json_string)
          .ToLocal(&json_dictionary))
    resolver_->Resolve(json_dictionary);
  else
    resolver_->Reject(exception_catcher.Exception());
  ClearResolver();
}

void CryptoResultImpl::CompleteWithBoolean(bool b) {
  if (!resolver_)
    return;

  resolver_->Resolve(b);
  ClearResolver();
}

void CryptoResultImpl::CompleteWithKey(const WebCryptoKey& key) {
  if (!resolver_)
    return;

  resolver_->Resolve(MakeGarbageCollected<CryptoKey>(key));
  ClearResolver();
}

void CryptoResultImpl::CompleteWithKeyPair(const WebCryptoKey& public_key,
                                           const WebCryptoKey& private_key) {
  if (!resolver_)
    return;

  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);

  V8ObjectBuilder key_pair(script_state);

  key_pair.Add("publicKey",
               ScriptValue::From(script_state,
                                 MakeGarbageCollected<CryptoKey>(public_key)));
  key_pair.Add("privateKey",
               ScriptValue::From(script_state,
                                 MakeGarbageCollected<CryptoKey>(private_key)));

  resolver_->Resolve(key_pair.V8Value());
  ClearResolver();
}

void CryptoResultImpl::Cancel() {
  DCHECK(cancel_);
  cancel_->Cancel();
  cancel_ = nullptr;
  ClearResolver();
}

ScriptPromise CryptoResultImpl::Promise() {
  return resolver_ ? resolver_->Promise() : ScriptPromise();
}

}  // namespace blink
