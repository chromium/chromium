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

#include "third_party/blink/renderer/modules/crypto/subtle_crypto.h"

#include "base/check_deref.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_crypto.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/web/web_crypto_histograms.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_json_web_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_arraybuffer_arraybufferview_jsonwebkey.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"
#include "third_party/blink/renderer/modules/crypto/crypto_utilities.h"
#include "third_party/blink/renderer/modules/crypto/normalize_algorithm.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

// Parses a JsonWebKey dictionary. On success writes the result to
// |jsonUtf8| as a UTF8-encoded JSON octet string and returns true.
// On failure sets an error on |result| and returns false.
//
// Note: The choice of output as an octet string is to facilitate interop
// with the non-JWK formats, but does mean there is a second parsing step.
// This design choice should be revisited after crbug.com/614385).
bool ParseJsonWebKey(const JsonWebKey& key, WebVector<uint8_t>& json_utf8) {
  auto json_object = std::make_unique<JSONObject>();

  if (key.hasKty())
    json_object->SetString("kty", key.kty());
  if (key.hasUse())
    json_object->SetString("use", key.use());
  if (key.hasKeyOps()) {
    auto json_array = std::make_unique<JSONArray>();
    for (auto&& value : key.keyOps())
      json_array->PushString(value);
    json_object->SetArray("key_ops", std::move(json_array));
  }
  if (key.hasAlg())
    json_object->SetString("alg", key.alg());
  if (key.hasExt())
    json_object->SetBoolean("ext", key.ext());

  if (key.hasCrv())
    json_object->SetString("crv", key.crv());
  if (key.hasX())
    json_object->SetString("x", key.x());
  if (key.hasY())
    json_object->SetString("y", key.y());
  if (key.hasD())
    json_object->SetString("d", key.d());
  if (key.hasN())
    json_object->SetString("n", key.n());
  if (key.hasE())
    json_object->SetString("e", key.e());
  if (key.hasP())
    json_object->SetString("p", key.p());
  if (key.hasQ())
    json_object->SetString("q", key.q());
  if (key.hasDp())
    json_object->SetString("dp", key.dp());
  if (key.hasDq())
    json_object->SetString("dq", key.dq());
  if (key.hasQi())
    json_object->SetString("qi", key.qi());
  // TODO(eroman): Parse "oth" (crbug.com/441396)
  if (key.hasK())
    json_object->SetString("k", key.k());

  String json = json_object->ToJSONString();
  json_utf8 = WebVector<uint8_t>(json.Utf8().c_str(), json.Utf8().length());
  return true;
}

}  // namespace

SubtleCrypto::SubtleCrypto() = default;

ScriptPromise<IDLAny> SubtleCrypto::encrypt(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* key,
    const V8BufferSource* raw_data,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-encrypt

  // 14.3.1.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the encrypt method.
  WebVector<uint8_t> data = CopyBytes(raw_data);

  // 14.3.1.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "encrypt".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationEncrypt, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.1.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.1.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "encrypt", then throw an InvalidAccessError.
  if (!key->CanBeUsedForAlgorithm(normalized_algorithm,
                                  kWebCryptoKeyUsageEncrypt, result))
    return promise;

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->Encrypt(normalized_algorithm, key->Key(),
                                         std::move(data), result->Result(),
                                         std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::decrypt(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* key,
    const V8BufferSource* raw_data,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-decrypt

  // 14.3.2.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the decrypt method.
  WebVector<uint8_t> data = CopyBytes(raw_data);

  // 14.3.2.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "decrypt".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationDecrypt, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.2.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.2.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "decrypt", then throw an InvalidAccessError.
  if (!key->CanBeUsedForAlgorithm(normalized_algorithm,
                                  kWebCryptoKeyUsageDecrypt, result))
    return promise;

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->Decrypt(normalized_algorithm, key->Key(),
                                         std::move(data), result->Result(),
                                         std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::sign(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* key,
    const V8BufferSource* raw_data,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-sign

  // 14.3.3.2: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the sign method.
  WebVector<uint8_t> data = CopyBytes(raw_data);

  // 14.3.3.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "sign".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationSign, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.3.8: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.3.9: If the [[usages]] internal slot of key does not contain an
  //           entry that is "sign", then throw an InvalidAccessError.
  if (!key->CanBeUsedForAlgorithm(normalized_algorithm, kWebCryptoKeyUsageSign,
                                  result))
    return promise;

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->Sign(normalized_algorithm, key->Key(),
                                      std::move(data), result->Result(),
                                      std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::verifySignature(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* key,
    const V8BufferSource* raw_signature,
    const V8BufferSource* raw_data,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-verify

  // 14.3.4.2: Let signature be the result of getting a copy of the bytes
  //           held by the signature parameter passed to the verify method.
  WebVector<uint8_t> signature = CopyBytes(raw_signature);

  // 14.3.4.3: Let data be the result of getting a copy of the bytes held by
  //           the data parameter passed to the verify method.
  WebVector<uint8_t> data = CopyBytes(raw_data);

  // 14.3.4.4: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "verify".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationVerify, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.4.9: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of key then
  //           throw an InvalidAccessError.
  //
  // 14.3.4.10: If the [[usages]] internal slot of key does not contain an
  //            entry that is "verify", then throw an InvalidAccessError.
  if (!key->CanBeUsedForAlgorithm(normalized_algorithm,
                                  kWebCryptoKeyUsageVerify, result))
    return promise;

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->VerifySignature(
      normalized_algorithm, key->Key(), std::move(signature), std::move(data),
      result->Result(), std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::digest(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    const V8BufferSource* raw_data,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-digest

  // 14.3.5.2: Let data be the result of getting a copy of the bytes held
  //              by the data parameter passed to the digest method.
  WebVector<uint8_t> data = CopyBytes(raw_data);

  // 14.3.5.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to "digest".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationDigest, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);

  HistogramAlgorithm(ExecutionContext::From(script_state),
                     normalized_algorithm);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->Digest(normalized_algorithm, std::move(data),
                                        result->Result(),
                                        std::move(task_runner));
  return resolver->Promise();
}

ScriptPromise<IDLAny> SubtleCrypto::generateKey(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    bool extractable,
    const Vector<String>& raw_key_usages,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-generateKey

  WebCryptoKeyUsageMask key_usages;
  if (!CryptoKey::ParseUsageMask(raw_key_usages, key_usages, exception_state)) {
    return EmptyPromise();
  }

  // 14.3.6.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "generateKey".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationGenerateKey, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // NOTE: Steps (8) and (9) disallow empty usages on secret and private
  // keys. This normative requirement is enforced by the platform
  // implementation in the call below.

  HistogramAlgorithm(ExecutionContext::From(script_state),
                     normalized_algorithm);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->GenerateKey(normalized_algorithm, extractable,
                                             key_usages, result->Result(),
                                             std::move(task_runner));
  return promise;
}

ScriptPromise<CryptoKey> SubtleCrypto::importKey(
    ScriptState* script_state,
    const String& raw_format,
    const V8UnionBufferSourceOrJsonWebKey* raw_key_data,
    const V8AlgorithmIdentifier* raw_algorithm,
    bool extractable,
    const Vector<String>& raw_key_usages,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-importKey
  WebCryptoKeyFormat format;
  if (!CryptoKey::ParseFormat(raw_format, format, exception_state)) {
    return EmptyPromise();
  }

  WebCryptoKeyUsageMask key_usages;
  if (!CryptoKey::ParseUsageMask(raw_key_usages, key_usages, exception_state)) {
    return EmptyPromise();
  }

  // In the case of JWK keyData will hold the UTF8-encoded JSON for the
  // JsonWebKey, otherwise it holds a copy of the BufferSource.
  WebVector<uint8_t> key_data;

  switch (format) {
    // 14.3.9.2: If format is equal to the string "raw", "pkcs8", or "spki":
    //
    //  (1) If the keyData parameter passed to the importKey method is a
    //      JsonWebKey dictionary, throw a TypeError.
    //
    //  (2) Let keyData be the result of getting a copy of the bytes held by
    //      the keyData parameter passed to the importKey method.
    case kWebCryptoKeyFormatRaw:
    case kWebCryptoKeyFormatPkcs8:
    case kWebCryptoKeyFormatSpki:
      switch (raw_key_data->GetContentType()) {
        case V8UnionBufferSourceOrJsonWebKey::ContentType::kArrayBuffer:
          key_data = CopyBytes(raw_key_data->GetAsArrayBuffer());
          break;
        case V8UnionBufferSourceOrJsonWebKey::ContentType::kArrayBufferView:
          key_data = CopyBytes(raw_key_data->GetAsArrayBufferView().Get());
          break;
        case V8UnionBufferSourceOrJsonWebKey::ContentType::kJsonWebKey:
          exception_state.ThrowTypeError(
              "Key data must be a BufferSource for non-JWK formats");
          return EmptyPromise();
      }
      break;
    // 14.3.9.2: If format is equal to the string "jwk":
    //
    //  (1) If the keyData parameter passed to the importKey method is not a
    //      JsonWebKey dictionary, throw a TypeError.
    //
    //  (2) Let keyData be the keyData parameter passed to the importKey
    //      method.
    case kWebCryptoKeyFormatJwk:
      if (!raw_key_data->IsJsonWebKey()) {
        exception_state.ThrowTypeError(
            "Key data must be an object for JWK import");
        return EmptyPromise();
      }
      if (!ParseJsonWebKey(*raw_key_data->GetAsJsonWebKey(), key_data)) {
        return EmptyPromise();
      }
      break;
  }

  // 14.3.9.3: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "importKey".

  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationImportKey, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<CryptoKey>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  HistogramAlgorithm(ExecutionContext::From(script_state),
                     normalized_algorithm);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->ImportKey(
      format, std::move(key_data), normalized_algorithm, extractable,
      key_usages, result->Result(), std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::exportKey(ScriptState* script_state,
                                              const String& raw_format,
                                              CryptoKey* key,
                                              ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-exportKey

  WebCryptoKeyFormat format;
  if (!CryptoKey::ParseFormat(raw_format, format, exception_state)) {
    return EmptyPromise();
  }

  // 14.3.10.6: If the [[extractable]] internal slot of key is false, then
  //            throw an InvalidAccessError.
  if (!key->extractable()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidAccessError,
                                      "key is not extractable");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  HistogramKey(ExecutionContext::From(script_state), key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->ExportKey(format, key->Key(), result->Result(),
                                           std::move(task_runner));
  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::wrapKey(
    ScriptState* script_state,
    const String& raw_format,
    CryptoKey* key,
    CryptoKey* wrapping_key,
    const V8AlgorithmIdentifier* raw_wrap_algorithm,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-wrapKey

  WebCryptoKeyFormat format;
  if (!CryptoKey::ParseFormat(raw_format, format, exception_state)) {
    return EmptyPromise();
  }

  // 14.3.11.2: Let normalizedAlgorithm be the result of normalizing an
  //            algorithm, with alg set to algorithm and op set to "wrapKey".
  //
  // 14.3.11.3: If an error occurred, let normalizedAlgorithm be the result
  //            of normalizing an algorithm, with alg set to algorithm and op
  //            set to "encrypt".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_wrap_algorithm,
                          kWebCryptoOperationWrapKey, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.11.9: If the name member of normalizedAlgorithm is not equal to the
  //            name attribute of the [[algorithm]] internal slot of
  //            wrappingKey then throw an InvalidAccessError.
  //
  // 14.3.11.10: If the [[usages]] internal slot of wrappingKey does not
  //             contain an entry that is "wrapKey", then throw an
  //             InvalidAccessError.
  if (!wrapping_key->CanBeUsedForAlgorithm(normalized_algorithm,
                                           kWebCryptoKeyUsageWrapKey, result))
    return promise;

  // TODO(crbug.com/628416): The error from step 11
  // (NotSupportedError) is thrown after step 12 which does not match
  // the spec order.

  // 14.3.11.12: If the [[extractable]] internal slot of key is false, then
  //             throw an InvalidAccessError.
  if (!key->extractable()) {
    result->CompleteWithError(kWebCryptoErrorTypeInvalidAccess,
                              "key is not extractable");
    return promise;
  }

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, wrapping_key->Key());
  HistogramKey(ExecutionContext::From(script_state), key->Key());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->WrapKey(
      format, key->Key(), wrapping_key->Key(), normalized_algorithm,
      result->Result(), std::move(task_runner));
  return promise;
}

ScriptPromise<CryptoKey> SubtleCrypto::unwrapKey(
    ScriptState* script_state,
    const String& raw_format,
    const V8BufferSource* raw_wrapped_key,
    CryptoKey* unwrapping_key,
    const V8AlgorithmIdentifier* raw_unwrap_algorithm,
    const V8AlgorithmIdentifier* raw_unwrapped_key_algorithm,
    bool extractable,
    const Vector<String>& raw_key_usages,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-unwrapKey

  WebCryptoKeyFormat format;
  if (!CryptoKey::ParseFormat(raw_format, format, exception_state)) {
    return EmptyPromise();
  }

  WebCryptoKeyUsageMask key_usages;
  if (!CryptoKey::ParseUsageMask(raw_key_usages, key_usages, exception_state)) {
    return EmptyPromise();
  }

  // 14.3.12.3: Let normalizedAlgorithm be the result of normalizing an
  //            algorithm, with alg set to algorithm and op set to
  //            "unwrapKey".
  //
  // 14.3.12.4: If an error occurred, let normalizedAlgorithm be the result
  //            of normalizing an algorithm, with alg set to algorithm and op
  //            set to "decrypt".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_unwrap_algorithm,
                          kWebCryptoOperationUnwrapKey, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  // 14.3.12.6: Let normalizedKeyAlgorithm be the result of normalizing an
  //            algorithm, with alg set to unwrappedKeyAlgorithm and op set
  //            to "importKey".
  WebCryptoAlgorithm normalized_key_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(),
                          raw_unwrapped_key_algorithm,
                          kWebCryptoOperationImportKey,
                          normalized_key_algorithm, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<CryptoKey>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.12.2: Let wrappedKey be the result of getting a copy of the bytes
  //            held by the wrappedKey parameter passed to the unwrapKey
  //            method.
  WebVector<uint8_t> wrapped_key = CopyBytes(raw_wrapped_key);

  // 14.3.12.11: If the name member of normalizedAlgorithm is not equal to
  //             the name attribute of the [[algorithm]] internal slot of
  //             unwrappingKey then throw an InvalidAccessError.
  //
  // 14.3.12.12: If the [[usages]] internal slot of unwrappingKey does not
  //             contain an entry that is "unwrapKey", then throw an
  //             InvalidAccessError.
  if (!unwrapping_key->CanBeUsedForAlgorithm(
          normalized_algorithm, kWebCryptoKeyUsageUnwrapKey, result))
    return promise;

  // NOTE: Step (16) disallows empty usages on secret and private keys. This
  // normative requirement is enforced by the platform implementation in the
  // call below.

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, unwrapping_key->Key());
  HistogramAlgorithm(ExecutionContext::From(script_state),
                     normalized_key_algorithm);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->UnwrapKey(
      format, std::move(wrapped_key), unwrapping_key->Key(),
      normalized_algorithm, normalized_key_algorithm, extractable, key_usages,
      result->Result(), std::move(task_runner));
  return promise;
}

ScriptPromise<DOMArrayBuffer> SubtleCrypto::deriveBits(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* base_key,
    std::optional<unsigned> length_bits,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#dfn-SubtleCrypto-method-deriveBits

  // 14.3.8.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "deriveBits".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationDeriveBits, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  // 14.3.8.7: If the name member of normalizedAlgorithm is not equal to the
  //           name attribute of the [[algorithm]] internal slot of baseKey
  //           then throw an InvalidAccessError.
  //
  // 14.3.8.8: If the [[usages]] internal slot of baseKey does not contain an
  //           entry that is "deriveBits", then throw an InvalidAccessError.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  if (!base_key->CanBeUsedForAlgorithm(normalized_algorithm,
                                       kWebCryptoKeyUsageDeriveBits, result)) {
    return promise;
  }

  auto* execution_context = ExecutionContext::From(script_state);
  HistogramAlgorithmAndKey(execution_context, normalized_algorithm,
                           base_key->Key());
  Platform::Current()->Crypto()->DeriveBits(
      normalized_algorithm, base_key->Key(), length_bits, result->Result(),
      execution_context->GetTaskRunner(blink::TaskType::kInternalWebCrypto));

  return promise;
}

ScriptPromise<IDLAny> SubtleCrypto::deriveKey(
    ScriptState* script_state,
    const V8AlgorithmIdentifier* raw_algorithm,
    CryptoKey* base_key,
    const V8AlgorithmIdentifier* raw_derived_key_type,
    bool extractable,
    const Vector<String>& raw_key_usages,
    ExceptionState& exception_state) {
  // Method described by:
  // https://w3c.github.io/webcrypto/Overview.html#SubtleCrypto-method-deriveKey

  WebCryptoKeyUsageMask key_usages;
  if (!CryptoKey::ParseUsageMask(raw_key_usages, key_usages, exception_state)) {
    return EmptyPromise();
  }

  // 14.3.7.2: Let normalizedAlgorithm be the result of normalizing an
  //           algorithm, with alg set to algorithm and op set to
  //           "deriveBits".
  WebCryptoAlgorithm normalized_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_algorithm,
                          kWebCryptoOperationDeriveBits, normalized_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  // 14.3.7.4: Let normalizedDerivedKeyAlgorithm be the result of normalizing
  //           an algorithm, with alg set to derivedKeyType and op set to
  //           "importKey".
  WebCryptoAlgorithm normalized_derived_key_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_derived_key_type,
                          kWebCryptoOperationImportKey,
                          normalized_derived_key_algorithm, exception_state)) {
    return EmptyPromise();
  }

  // TODO(eroman): The description in the spec needs to be updated as
  // it doesn't describe algorithm normalization for the Get Key
  // Length parameters (https://github.com/w3c/webcrypto/issues/127)
  // For now reference step 10 which is the closest.
  //
  // 14.3.7.10: If the name member of normalizedDerivedKeyAlgorithm does not
  //            identify a registered algorithm that supports the get key length
  //            operation, then throw a NotSupportedError.
  WebCryptoAlgorithm key_length_algorithm;
  if (!NormalizeAlgorithm(script_state->GetIsolate(), raw_derived_key_type,
                          kWebCryptoOperationGetKeyLength, key_length_algorithm,
                          exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  auto promise = resolver->Promise();

  // 14.3.7.11: If the name member of normalizedAlgorithm is not equal to the
  //            name attribute of the [[algorithm]] internal slot of baseKey
  //            then throw an InvalidAccessError.
  //
  // 14.3.7.12: If the [[usages]] internal slot of baseKey does not contain
  //            an entry that is "deriveKey", then throw an InvalidAccessError.
  if (!base_key->CanBeUsedForAlgorithm(normalized_algorithm,
                                       kWebCryptoKeyUsageDeriveKey, result))
    return promise;

  // NOTE: Step (16) disallows empty usages on secret and private keys. This
  // normative requirement is enforced by the platform implementation in the
  // call below.

  HistogramAlgorithmAndKey(ExecutionContext::From(script_state),
                           normalized_algorithm, base_key->Key());
  HistogramAlgorithm(ExecutionContext::From(script_state),
                     normalized_derived_key_algorithm);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ExecutionContext::From(script_state)
          ->GetTaskRunner(blink::TaskType::kInternalWebCrypto);
  Platform::Current()->Crypto()->DeriveKey(
      normalized_algorithm, base_key->Key(), normalized_derived_key_algorithm,
      key_length_algorithm, extractable, key_usages, result->Result(),
      std::move(task_runner));
  return promise;
}

}  // namespace blink
