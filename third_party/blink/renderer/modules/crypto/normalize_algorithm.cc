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

#include "third_party/blink/renderer/modules/crypto/normalize_algorithm.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>

#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_object_string.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/crypto/crypto_utilities.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

struct AlgorithmNameMapping {
  // Must be an upper case ASCII string.
  const char* const algorithm_name;
  // Must be strlen(algorithmName).
  unsigned char algorithm_name_length;
  WebCryptoAlgorithmId algorithm_id;

#if DCHECK_IS_ON()
  bool operator<(const AlgorithmNameMapping&) const;
#endif
};

constexpr size_t kAlgorithmNameMappingsSize = kWebCryptoAlgorithmIdLast + 1;

using AlgorithmNameMappingArray =
    std::array<AlgorithmNameMapping, kAlgorithmNameMappingsSize>;

// Must be sorted by length, and then by reverse string.
// Also all names must be upper case ASCII.
constexpr AlgorithmNameMappingArray kAlgorithmNameMappings = {{
    {"HMAC", 4, kWebCryptoAlgorithmIdHmac},
    {"HKDF", 4, kWebCryptoAlgorithmIdHkdf},
    {"ECDH", 4, kWebCryptoAlgorithmIdEcdh},
    {"SHA-1", 5, kWebCryptoAlgorithmIdSha1},
    {"ECDSA", 5, kWebCryptoAlgorithmIdEcdsa},
    {"PBKDF2", 6, kWebCryptoAlgorithmIdPbkdf2},
    {"X25519", 6, kWebCryptoAlgorithmIdX25519},
    {"AES-KW", 6, kWebCryptoAlgorithmIdAesKw},
    {"SHA-512", 7, kWebCryptoAlgorithmIdSha512},
    {"SHA-384", 7, kWebCryptoAlgorithmIdSha384},
    {"SHA-256", 7, kWebCryptoAlgorithmIdSha256},
    {"ED25519", 7, kWebCryptoAlgorithmIdEd25519},
    {"AES-CBC", 7, kWebCryptoAlgorithmIdAesCbc},
    {"AES-GCM", 7, kWebCryptoAlgorithmIdAesGcm},
    {"AES-CTR", 7, kWebCryptoAlgorithmIdAesCtr},
    {"RSA-PSS", 7, kWebCryptoAlgorithmIdRsaPss},
    {"RSA-OAEP", 8, kWebCryptoAlgorithmIdRsaOaep},
    {"ML-DSA-44", 9, kWebCryptoAlgorithmIdMlDsa44},
    {"ML-DSA-65", 9, kWebCryptoAlgorithmIdMlDsa65},
    {"ML-DSA-87", 9, kWebCryptoAlgorithmIdMlDsa87},
    {"ML-KEM-768", 10, kWebCryptoAlgorithmIdMlKem768},
    {"ML-KEM-1024", 11, kWebCryptoAlgorithmIdMlKem1024},
    {"CHACHA20-POLY1305", 17, kWebCryptoAlgorithmIdChaCha20Poly1305},
    {"RSASSA-PKCS1-V1_5", 17, kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5},
}};

// Reminder to update the table mapping names to IDs whenever adding a new
// algorithm ID.
static_assert(kAlgorithmNameMappingsSize == std::size(kAlgorithmNameMappings),
              "algorithmNameMappings needs to be updated");

#if DCHECK_IS_ON()

constexpr bool LessThanReverse(std::string_view a, std::string_view b) {
  if (a.length() < b.length()) {
    return true;
  }
  if (a.length() > b.length()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    const size_t reverse_index = a.size() - i - 1;
    const char c1 = a[reverse_index];
    const char c2 = b[reverse_index];
    if (c1 < c2) {
      return true;
    }
    if (c1 > c2) {
      return false;
    }
  }
  return false;
}

constexpr bool VerifyAlgorithmNameMappings() {
  for (const auto& algo : kAlgorithmNameMappings) {
    if (algo.algorithm_name_length == 0) {
      return false;
    }
    std::string_view name(algo.algorithm_name);
    if (algo.algorithm_name_length != name.length()) {
      return false;
    }
    auto is_valid_algorithm_char = [](char c) {
      return IsASCII(c) && c == ToASCIIUpper(c);
    };
    if (!std::ranges::all_of(name, is_valid_algorithm_char)) {
      return false;
    }
  }
  return std::ranges::is_sorted(
      kAlgorithmNameMappings, LessThanReverse,
      [](const AlgorithmNameMapping& o) {
        return std::string_view(o.algorithm_name, o.algorithm_name_length);
      });
}
static_assert(VerifyAlgorithmNameMappings(),
              "algorithm mapping is malformed: not sorted or contains "
              "non-uppercase ASCII");
#endif

template <typename CharType>
bool AlgorithmNameComparator(const AlgorithmNameMapping& a,
                             base::span<const CharType> b) {
  if (a.algorithm_name_length < b.size()) {
    return true;
  }
  if (a.algorithm_name_length > b.size()) {
    return false;
  }

  // Because the algorithm names contain many common prefixes, it is better
  // to compare starting at the end of the string.
  const std::string_view a_name(a.algorithm_name, a.algorithm_name_length);
  for (size_t i = 0; i < a_name.size(); ++i) {
    const size_t reverse_index = a_name.size() - i - 1;
    CharType c2 = b[reverse_index];
    if (!IsASCII(c2))
      return false;
    c2 = ToASCIIUpper(c2);

    const CharType c1 = a_name[reverse_index];
    if (c1 < c2)
      return true;
    if (c1 > c2)
      return false;
  }
  return false;
}

std::optional<WebCryptoAlgorithmId> LookupAlgorithmIdByName(
    const String& algorithm_name) {
  auto it = VisitCharacters(algorithm_name, [&](auto algo_chars) {
    using CharType = decltype(algo_chars)::value_type;
    auto begin = kAlgorithmNameMappings.begin();
    auto end = kAlgorithmNameMappings.end();
    return std::lower_bound(begin, end, algo_chars,
                            AlgorithmNameComparator<CharType>);
  });

  if (it == kAlgorithmNameMappings.end()) {
    return std::nullopt;
  }

  if (it->algorithm_name_length != algorithm_name.length() ||
      !DeprecatedEqualIgnoringCase(algorithm_name, it->algorithm_name))
    return std::nullopt;

  WebCryptoAlgorithmId id = it->algorithm_id;

  if ((id == kWebCryptoAlgorithmIdChaCha20Poly1305 ||
       id == kWebCryptoAlgorithmIdMlDsa44 ||
       id == kWebCryptoAlgorithmIdMlDsa65 ||
       id == kWebCryptoAlgorithmIdMlDsa87 ||
       id == kWebCryptoAlgorithmIdMlKem768 ||
       id == kWebCryptoAlgorithmIdMlKem1024) &&
      !RuntimeEnabledFeatures::WebCryptoPQCEnabled()) {
    return std::nullopt;
  }
  return id;
}

void SetTypeError(const String& message, ExceptionState& exception_state) {
  exception_state.ThrowTypeError(message);
}

void SetNotSupportedError(const String& message,
                          ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    message);
}

// ErrorContext holds a stack of string literals which describe what was
// happening at the time the error occurred. This is helpful because
// parsing of the algorithm dictionary can be recursive and it is difficult to
// tell what went wrong from a failure alone.
class ErrorContext {
  STACK_ALLOCATED();

 public:
  void Add(const char* message) { messages_.push_back(message); }

  void RemoveLast() { messages_.pop_back(); }

  // Join all of the string literals into a single String.
  String ToString() const {
    if (messages_.empty())
      return String();

    StringBuilder result;
    const base::span<const LChar> separator =
        base::byte_span_from_cstring(": ");

    wtf_size_t length = (messages_.size() - 1) * separator.size();
    for (wtf_size_t i = 0; i < messages_.size(); ++i)
      length += strlen(messages_[i]);
    result.ReserveCapacity(length);

    for (wtf_size_t i = 0; i < messages_.size(); ++i) {
      if (i)
        result.Append(separator);
      result.Append(StringView(messages_[i]));
    }

    return result.ToString();
  }

  String ToString(const char* message) const {
    ErrorContext stack(*this);
    stack.Add(message);
    return stack.ToString();
  }

  String ToString(const char* message1, const char* message2) const {
    ErrorContext stack(*this);
    stack.Add(message1);
    stack.Add(message2);
    return stack.ToString();
  }

 private:
  // This inline size is large enough to avoid having to grow the Vector in
  // the majority of cases (up to 1 nested algorithm identifier).
  Vector<const char*, 10> messages_;
};

// Defined by the WebCrypto spec as:
//
//     typedef (ArrayBuffer or ArrayBufferView) BufferSource;
//
bool GetOptionalBufferSource(const Dictionary& raw,
                             const char* property_name,
                             bool& has_property,
                             std::vector<uint8_t>& bytes,
                             const ErrorContext& context,
                             ExceptionState& exception_state) {
  has_property = false;
  v8::Local<v8::Value> v8_value;
  if (!raw.Get(property_name, v8_value))
    return true;
  has_property = true;

  if (v8_value->IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view =
        NativeValueTraits<NotShared<DOMArrayBufferView>>::NativeValue(
            raw.GetIsolate(), v8_value, exception_state)
            .Get();
    if (exception_state.HadException())
      return false;
    bytes = CopyBytes(array_buffer_view);
    return true;
  }

  if (v8_value->IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer =
        NativeValueTraits<DOMArrayBuffer>::NativeValue(
            raw.GetIsolate(), v8_value, exception_state);
    if (exception_state.HadException())
      return false;
    bytes = CopyBytes(array_buffer);
    return true;
  }

  if (has_property) {
    SetTypeError(context.ToString(property_name, "Not a BufferSource"),
                 exception_state);
    return false;
  }
  return true;
}

bool GetBufferSource(const Dictionary& raw,
                     const char* property_name,
                     std::vector<uint8_t>& bytes,
                     const ErrorContext& context,
                     ExceptionState& exception_state) {
  bool has_property;
  bool ok = GetOptionalBufferSource(raw, property_name, has_property, bytes,
                                    context, exception_state);
  if (!has_property) {
    SetTypeError(context.ToString(property_name, "Missing required property"),
                 exception_state);
    return false;
  }
  return ok;
}

bool GetUint8Array(const Dictionary& raw,
                   const char* property_name,
                   std::vector<uint8_t>& bytes,
                   const ErrorContext& context,
                   ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value;
  if (!raw.Get(property_name, v8_value) || !v8_value->IsUint8Array()) {
    SetTypeError(context.ToString(property_name, "Missing or not a Uint8Array"),
                 exception_state);
    return false;
  }

  MaybeShared<DOMUint8Array> array =
      NativeValueTraits<MaybeShared<DOMUint8Array>>::NativeValue(
          raw.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException()) {
    return false;
  }
  bytes = CopyBytes(array.Get());
  return true;
}

// Defined by the WebCrypto spec as:
//
//     typedef Uint8Array BigInteger;
bool GetBigInteger(const Dictionary& raw,
                   const char* property_name,
                   std::vector<uint8_t>& bytes,
                   const ErrorContext& context,
                   ExceptionState& exception_state) {
  if (!GetUint8Array(raw, property_name, bytes, context, exception_state))
    return false;

  if (bytes.empty()) {
    // Empty BigIntegers represent 0 according to the spec
    bytes = std::vector<uint8_t>(static_cast<size_t>(1u));
    DCHECK_EQ(0u, bytes[0]);
  }

  return true;
}

// Gets an integer according to WebIDL's [EnforceRange].
bool GetOptionalInteger(const Dictionary& raw,
                        const char* property_name,
                        bool& has_property,
                        double& value,
                        double min_value,
                        double max_value,
                        const ErrorContext& context,
                        ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value;
  if (!raw.Get(property_name, v8_value)) {
    has_property = false;
    return true;
  }

  has_property = true;
  double number;
  bool ok = v8_value->NumberValue(raw.V8Context()).To(&number);

  if (!ok || std::isnan(number)) {
    SetTypeError(context.ToString(property_name, "Is not a number"),
                 exception_state);
    return false;
  }

  number = trunc(number);

  if (std::isinf(number) || number < min_value || number > max_value) {
    SetTypeError(context.ToString(property_name, "Outside of numeric range"),
                 exception_state);
    return false;
  }

  value = number;
  return true;
}

bool GetInteger(const Dictionary& raw,
                const char* property_name,
                double& value,
                double min_value,
                double max_value,
                const ErrorContext& context,
                ExceptionState& exception_state) {
  bool has_property;
  if (!GetOptionalInteger(raw, property_name, has_property, value, min_value,
                          max_value, context, exception_state))
    return false;

  if (!has_property) {
    SetTypeError(context.ToString(property_name, "Missing required property"),
                 exception_state);
    return false;
  }

  return true;
}

bool GetUint32(const Dictionary& raw,
               const char* property_name,
               uint32_t& value,
               const ErrorContext& context,
               ExceptionState& exception_state) {
  double number;
  if (!GetInteger(raw, property_name, number, 0, 0xFFFFFFFF, context,
                  exception_state))
    return false;
  value = number;
  return true;
}

bool GetUint16(const Dictionary& raw,
               const char* property_name,
               uint16_t& value,
               const ErrorContext& context,
               ExceptionState& exception_state) {
  double number;
  if (!GetInteger(raw, property_name, number, 0, 0xFFFF, context,
                  exception_state))
    return false;
  value = number;
  return true;
}

bool GetUint8(const Dictionary& raw,
              const char* property_name,
              uint8_t& value,
              const ErrorContext& context,
              ExceptionState& exception_state) {
  double number;
  if (!GetInteger(raw, property_name, number, 0, 0xFF, context,
                  exception_state))
    return false;
  value = number;
  return true;
}

bool GetOptionalUint32(const Dictionary& raw,
                       const char* property_name,
                       bool& has_value,
                       uint32_t& value,
                       const ErrorContext& context,
                       ExceptionState& exception_state) {
  double number;
  if (!GetOptionalInteger(raw, property_name, has_value, number, 0, 0xFFFFFFFF,
                          context, exception_state))
    return false;
  if (has_value)
    value = number;
  return true;
}

bool GetOptionalUint8(const Dictionary& raw,
                      const char* property_name,
                      bool& has_value,
                      uint8_t& value,
                      const ErrorContext& context,
                      ExceptionState& exception_state) {
  double number;
  if (!GetOptionalInteger(raw, property_name, has_value, number, 0, 0xFF,
                          context, exception_state))
    return false;
  if (has_value)
    value = number;
  return true;
}

V8AlgorithmIdentifier* GetAlgorithmIdentifier(v8::Isolate* isolate,
                                              const Dictionary& raw,
                                              const char* property_name,
                                              const ErrorContext& context,
                                              ExceptionState& exception_state) {
  // FIXME: This is not correct: http://crbug.com/438060
  //   (1) It may retrieve the property twice from the dictionary, whereas it
  //       should be reading the v8 value once to avoid issues with getters.
  //   (2) The value is stringified (whereas the spec says it should be an
  //       instance of DOMString).
  Dictionary dictionary;
  if (raw.Get(property_name, dictionary) && dictionary.IsObject()) {
    return MakeGarbageCollected<V8AlgorithmIdentifier>(
        ScriptObject(isolate, dictionary.V8Value().As<v8::Object>()));
  }

  std::optional<String> algorithm_name =
      raw.Get<IDLString>(property_name, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (!algorithm_name.has_value()) {
    SetTypeError(context.ToString(property_name,
                                  "Missing or not an AlgorithmIdentifier"),
                 exception_state);
    return nullptr;
  }

  return MakeGarbageCollected<V8AlgorithmIdentifier>(*algorithm_name);
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCbcParams : Algorithm {
//      required BufferSource iv;
//    };
bool ParseAesCbcParams(const Dictionary& raw,
                       std::unique_ptr<WebCryptoAlgorithmParams>& params,
                       const ErrorContext& context,
                       ExceptionState& exception_state) {
  std::vector<uint8_t> iv;
  if (!GetBufferSource(raw, "iv", iv, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoAesCbcParams>(std::move(iv));
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesKeyGenParams : Algorithm {
//      [EnforceRange] required unsigned short length;
//    };
bool ParseAesKeyGenParams(const Dictionary& raw,
                          std::unique_ptr<WebCryptoAlgorithmParams>& params,
                          const ErrorContext& context,
                          ExceptionState& exception_state) {
  uint16_t length;
  if (!GetUint16(raw, "length", length, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoAesKeyGenParams>(length);
  return true;
}

bool ParseAlgorithmIdentifier(v8::Isolate*,
                              const V8AlgorithmIdentifier&,
                              WebCryptoOperation,
                              WebCryptoAlgorithm&,
                              ErrorContext,
                              ExceptionState&);

bool ParseHash(v8::Isolate* isolate,
               const Dictionary& raw,
               WebCryptoAlgorithm& hash,
               ErrorContext context,
               ExceptionState& exception_state) {
  V8AlgorithmIdentifier* raw_hash =
      GetAlgorithmIdentifier(isolate, raw, "hash", context, exception_state);
  if (!raw_hash) {
    DCHECK(exception_state.HadException());
    return false;
  }

  context.Add("hash");
  return ParseAlgorithmIdentifier(isolate, *raw_hash, kWebCryptoOperationDigest,
                                  hash, context, exception_state);
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacImportParams : Algorithm {
//      required HashAlgorithmIdentifier hash;
//      [EnforceRange] unsigned long length;
//    };
bool ParseHmacImportParams(v8::Isolate* isolate,
                           const Dictionary& raw,
                           std::unique_ptr<WebCryptoAlgorithmParams>& params,
                           const ErrorContext& context,
                           ExceptionState& exception_state) {
  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;

  bool has_length;
  uint32_t length = 0;
  if (!GetOptionalUint32(raw, "length", has_length, length, context,
                         exception_state))
    return false;

  params =
      std::make_unique<WebCryptoHmacImportParams>(hash, has_length, length);
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary HmacKeyGenParams : Algorithm {
//      required HashAlgorithmIdentifier hash;
//      [EnforceRange] unsigned long length;
//    };
bool ParseHmacKeyGenParams(v8::Isolate* isolate,
                           const Dictionary& raw,
                           std::unique_ptr<WebCryptoAlgorithmParams>& params,
                           const ErrorContext& context,
                           ExceptionState& exception_state) {
  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;

  bool has_length;
  uint32_t length = 0;
  if (!GetOptionalUint32(raw, "length", has_length, length, context,
                         exception_state))
    return false;

  params =
      std::make_unique<WebCryptoHmacKeyGenParams>(hash, has_length, length);
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaHashedImportParams : Algorithm {
//      required HashAlgorithmIdentifier hash;
//    };
bool ParseRsaHashedImportParams(
    v8::Isolate* isolate,
    const Dictionary& raw,
    std::unique_ptr<WebCryptoAlgorithmParams>& params,
    const ErrorContext& context,
    ExceptionState& exception_state) {
  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoRsaHashedImportParams>(hash);
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary RsaKeyGenParams : Algorithm {
//      [EnforceRange] required unsigned long modulusLength;
//      required BigInteger publicExponent;
//    };
//
//    dictionary RsaHashedKeyGenParams : RsaKeyGenParams {
//      required HashAlgorithmIdentifier hash;
//    };
bool ParseRsaHashedKeyGenParams(
    v8::Isolate* isolate,
    const Dictionary& raw,
    std::unique_ptr<WebCryptoAlgorithmParams>& params,
    const ErrorContext& context,
    ExceptionState& exception_state) {
  uint32_t modulus_length;
  if (!GetUint32(raw, "modulusLength", modulus_length, context,
                 exception_state))
    return false;

  std::vector<uint8_t> public_exponent;
  if (!GetBigInteger(raw, "publicExponent", public_exponent, context,
                     exception_state))
    return false;

  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoRsaHashedKeyGenParams>(
      hash, modulus_length, std::move(public_exponent));
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesCtrParams : Algorithm {
//      required BufferSource counter;
//      [EnforceRange] required octet length;
//    };
bool ParseAesCtrParams(const Dictionary& raw,
                       std::unique_ptr<WebCryptoAlgorithmParams>& params,
                       const ErrorContext& context,
                       ExceptionState& exception_state) {
  std::vector<uint8_t> counter;
  if (!GetBufferSource(raw, "counter", counter, context, exception_state))
    return false;

  uint8_t length;
  if (!GetUint8(raw, "length", length, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoAesCtrParams>(length, std::move(counter));
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary AeadParams : Algorithm {
//       required BufferSource iv;
//       BufferSource additionalData;
//       [EnforceRange] octet tagLength;
//     }
bool ParseAeadParams(const Dictionary& raw,
                     std::unique_ptr<WebCryptoAlgorithmParams>& params,
                     const ErrorContext& context,
                     ExceptionState& exception_state) {
  std::vector<uint8_t> iv;
  if (!GetBufferSource(raw, "iv", iv, context, exception_state))
    return false;

  bool has_additional_data;
  std::vector<uint8_t> additional_data;
  if (!GetOptionalBufferSource(raw, "additionalData", has_additional_data,
                               additional_data, context, exception_state))
    return false;

  uint8_t tag_length = 0;
  bool has_tag_length;
  if (!GetOptionalUint8(raw, "tagLength", has_tag_length, tag_length, context,
                        exception_state))
    return false;

  params = std::make_unique<WebCryptoAeadParams>(
      std::move(iv), has_additional_data, std::move(additional_data),
      has_tag_length, tag_length);
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary RsaOaepParams : Algorithm {
//       BufferSource label;
//     };
bool ParseRsaOaepParams(const Dictionary& raw,
                        std::unique_ptr<WebCryptoAlgorithmParams>& params,
                        const ErrorContext& context,
                        ExceptionState& exception_state) {
  bool has_label;
  std::vector<uint8_t> label;
  if (!GetOptionalBufferSource(raw, "label", has_label, label, context,
                               exception_state))
    return false;

  params =
      std::make_unique<WebCryptoRsaOaepParams>(has_label, std::move(label));
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary RsaPssParams : Algorithm {
//       [EnforceRange] required unsigned long saltLength;
//     };
bool ParseRsaPssParams(const Dictionary& raw,
                       std::unique_ptr<WebCryptoAlgorithmParams>& params,
                       const ErrorContext& context,
                       ExceptionState& exception_state) {
  uint32_t salt_length_bytes;
  if (!GetUint32(raw, "saltLength", salt_length_bytes, context,
                 exception_state))
    return false;

  params = std::make_unique<WebCryptoRsaPssParams>(salt_length_bytes);
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary EcdsaParams : Algorithm {
//       required HashAlgorithmIdentifier hash;
//     };
bool ParseEcdsaParams(v8::Isolate* isolate,
                      const Dictionary& raw,
                      std::unique_ptr<WebCryptoAlgorithmParams>& params,
                      const ErrorContext& context,
                      ExceptionState& exception_state) {
  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoEcdsaParams>(hash);
  return true;
}

struct CurveNameMapping {
  const char* const name;
  WebCryptoNamedCurve value;
};

const CurveNameMapping kCurveNameMappings[] = {
    {"P-256", kWebCryptoNamedCurveP256},
    {"P-384", kWebCryptoNamedCurveP384},
    {"P-521", kWebCryptoNamedCurveP521}};

// Reminder to update curveNameMappings when adding a new curve.
static_assert(kWebCryptoNamedCurveLast + 1 == std::size(kCurveNameMappings),
              "curveNameMappings needs to be updated");

bool ParseNamedCurve(const Dictionary& raw,
                     WebCryptoNamedCurve& named_curve,
                     ErrorContext context,
                     ExceptionState& exception_state) {
  std::optional<String> named_curve_string =
      raw.Get<IDLString>("namedCurve", exception_state);
  if (exception_state.HadException()) {
    return false;
  }

  if (!named_curve_string.has_value()) {
    SetTypeError(context.ToString("namedCurve", "Missing or not a string"),
                 exception_state);
    return false;
  }

  for (const auto& curve_name_mapping : kCurveNameMappings) {
    if (curve_name_mapping.name == *named_curve_string) {
      named_curve = curve_name_mapping.value;
      return true;
    }
  }

  SetNotSupportedError(context.ToString("Unrecognized namedCurve"),
                       exception_state);
  return false;
}

// Defined by the WebCrypto spec as:
//
//     dictionary EcKeyGenParams : Algorithm {
//       required NamedCurve namedCurve;
//     };
bool ParseEcKeyGenParams(const Dictionary& raw,
                         std::unique_ptr<WebCryptoAlgorithmParams>& params,
                         const ErrorContext& context,
                         ExceptionState& exception_state) {
  WebCryptoNamedCurve named_curve;
  if (!ParseNamedCurve(raw, named_curve, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoEcKeyGenParams>(named_curve);
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary EcKeyImportParams : Algorithm {
//       required NamedCurve namedCurve;
//     };
bool ParseEcKeyImportParams(const Dictionary& raw,
                            std::unique_ptr<WebCryptoAlgorithmParams>& params,
                            const ErrorContext& context,
                            ExceptionState& exception_state) {
  WebCryptoNamedCurve named_curve;
  if (!ParseNamedCurve(raw, named_curve, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoEcKeyImportParams>(named_curve);
  return true;
}

bool GetPeerPublicKey(const Dictionary& raw,
                      const ErrorContext& context,
                      WebCryptoKey* peer_public_key,
                      ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value;
  if (!raw.Get("public", v8_value)) {
    SetTypeError(context.ToString("public", "Missing required property"),
                 exception_state);
    return false;
  }

  CryptoKey* crypto_key = V8CryptoKey::ToWrappable(raw.GetIsolate(), v8_value);
  if (!crypto_key) {
    SetTypeError(context.ToString("public", "Must be a CryptoKey"),
                 exception_state);
    return false;
  }

  *peer_public_key = crypto_key->Key();
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary EcdhKeyDeriveParams : Algorithm {
//       required CryptoKey public;
//     };
bool ParseEcdhKeyDeriveParams(const Dictionary& raw,
                              std::unique_ptr<WebCryptoAlgorithmParams>& params,
                              const ErrorContext& context,
                              ExceptionState& exception_state) {
  WebCryptoKey peer_public_key;
  if (!GetPeerPublicKey(raw, context, &peer_public_key, exception_state))
    return false;

  DCHECK(!peer_public_key.IsNull());
  params = std::make_unique<WebCryptoEcdhKeyDeriveParams>(peer_public_key);
  return true;
}

// Defined by the WebCrypto spec as:
//
//     dictionary Pbkdf2Params : Algorithm {
//       required BufferSource salt;
//       [EnforceRange] required unsigned long iterations;
//       required HashAlgorithmIdentifier hash;
//     };
bool ParsePbkdf2Params(v8::Isolate* isolate,
                       const Dictionary& raw,
                       std::unique_ptr<WebCryptoAlgorithmParams>& params,
                       const ErrorContext& context,
                       ExceptionState& exception_state) {
  std::vector<uint8_t> salt;
  if (!GetBufferSource(raw, "salt", salt, context, exception_state))
    return false;

  uint32_t iterations;
  if (!GetUint32(raw, "iterations", iterations, context, exception_state))
    return false;

  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;
  params = std::make_unique<WebCryptoPbkdf2Params>(hash, std::move(salt),
                                                   iterations);
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary AesDerivedKeyParams : Algorithm {
//      [EnforceRange] required unsigned short length;
//    };
bool ParseAesDerivedKeyParams(const Dictionary& raw,
                              std::unique_ptr<WebCryptoAlgorithmParams>& params,
                              const ErrorContext& context,
                              ExceptionState& exception_state) {
  uint16_t length;
  if (!GetUint16(raw, "length", length, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoAesDerivedKeyParams>(length);
  return true;
}

// Defined by the WebCrypto spec as:
//
//    dictionary HkdfParams : Algorithm {
//      required HashAlgorithmIdentifier hash;
//      required BufferSource salt;
//      required BufferSource info;
//    };
bool ParseHkdfParams(v8::Isolate* isolate,
                     const Dictionary& raw,
                     std::unique_ptr<WebCryptoAlgorithmParams>& params,
                     const ErrorContext& context,
                     ExceptionState& exception_state) {
  WebCryptoAlgorithm hash;
  if (!ParseHash(isolate, raw, hash, context, exception_state))
    return false;
  std::vector<uint8_t> salt;
  if (!GetBufferSource(raw, "salt", salt, context, exception_state))
    return false;
  std::vector<uint8_t> info;
  if (!GetBufferSource(raw, "info", info, context, exception_state))
    return false;

  params = std::make_unique<WebCryptoHkdfParams>(hash, std::move(salt),
                                                 std::move(info));
  return true;
}

bool ParseAlgorithmParams(v8::Isolate* isolate,
                          const Dictionary& raw,
                          WebCryptoAlgorithmParamsType type,
                          std::unique_ptr<WebCryptoAlgorithmParams>& params,
                          ErrorContext& context,
                          ExceptionState& exception_state) {
  switch (type) {
    case kWebCryptoAlgorithmParamsTypeNone:
      return true;
    case kWebCryptoAlgorithmParamsTypeAesCbcParams:
      context.Add("AesCbcParams");
      return ParseAesCbcParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeAesKeyGenParams:
      context.Add("AesKeyGenParams");
      return ParseAesKeyGenParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeHmacImportParams:
      context.Add("HmacImportParams");
      return ParseHmacImportParams(isolate, raw, params, context,
                                   exception_state);
    case kWebCryptoAlgorithmParamsTypeHmacKeyGenParams:
      context.Add("HmacKeyGenParams");
      return ParseHmacKeyGenParams(isolate, raw, params, context,
                                   exception_state);
    case kWebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      context.Add("RsaHashedKeyGenParams");
      return ParseRsaHashedKeyGenParams(isolate, raw, params, context,
                                        exception_state);
    case kWebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      context.Add("RsaHashedImportParams");
      return ParseRsaHashedImportParams(isolate, raw, params, context,
                                        exception_state);
    case kWebCryptoAlgorithmParamsTypeAesCtrParams:
      context.Add("AesCtrParams");
      return ParseAesCtrParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeAeadParams:
      context.Add("AeadParams");
      return ParseAeadParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeRsaOaepParams:
      context.Add("RsaOaepParams");
      return ParseRsaOaepParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeRsaPssParams:
      context.Add("RsaPssParams");
      return ParseRsaPssParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeEcdsaParams:
      context.Add("EcdsaParams");
      return ParseEcdsaParams(isolate, raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeEcKeyGenParams:
      context.Add("EcKeyGenParams");
      return ParseEcKeyGenParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeEcKeyImportParams:
      context.Add("EcKeyImportParams");
      return ParseEcKeyImportParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeEcdhKeyDeriveParams:
      context.Add("EcdhKeyDeriveParams");
      return ParseEcdhKeyDeriveParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeAesDerivedKeyParams:
      context.Add("AesDerivedKeyParams");
      return ParseAesDerivedKeyParams(raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypeHkdfParams:
      context.Add("HkdfParams");
      return ParseHkdfParams(isolate, raw, params, context, exception_state);
    case kWebCryptoAlgorithmParamsTypePbkdf2Params:
      context.Add("Pbkdf2Params");
      return ParsePbkdf2Params(isolate, raw, params, context, exception_state);
  }
  NOTREACHED();
}

const char* OperationToString(WebCryptoOperation op) {
  switch (op) {
    case kWebCryptoOperationEncrypt:
      return "encrypt";
    case kWebCryptoOperationDecrypt:
      return "decrypt";
    case kWebCryptoOperationSign:
      return "sign";
    case kWebCryptoOperationVerify:
      return "verify";
    case kWebCryptoOperationDigest:
      return "digest";
    case kWebCryptoOperationGenerateKey:
      return "generateKey";
    case kWebCryptoOperationImportKey:
      return "importKey";
    case kWebCryptoOperationGetKeyLength:
      return "get key length";
    case kWebCryptoOperationDeriveBits:
      return "deriveBits";
    case kWebCryptoOperationWrapKey:
      return "wrapKey";
    case kWebCryptoOperationUnwrapKey:
      return "unwrapKey";
  }
  return nullptr;
}

bool ParseAlgorithmDictionary(v8::Isolate* isolate,
                              const String& algorithm_name,
                              const Dictionary& raw,
                              WebCryptoOperation op,
                              WebCryptoAlgorithm& algorithm,
                              ErrorContext context,
                              ExceptionState& exception_state) {
  std::optional<WebCryptoAlgorithmId> algorithm_id =
      LookupAlgorithmIdByName(algorithm_name);
  if (!algorithm_id) {
    SetNotSupportedError(context.ToString("Unrecognized name"),
                         exception_state);
    return false;
  }

  // Remove the "Algorithm:" prefix for all subsequent errors.
  context.RemoveLast();

  const WebCryptoAlgorithmInfo* algorithm_info =
      WebCryptoAlgorithm::LookupAlgorithmInfo(*algorithm_id);

  if (algorithm_info->operation_to_params_type[op] ==
      WebCryptoAlgorithmInfo::kUndefined) {
    context.Add(algorithm_info->name);
    SetNotSupportedError(
        context.ToString("Unsupported operation", OperationToString(op)),
        exception_state);
    return false;
  }

  WebCryptoAlgorithmParamsType params_type =
      static_cast<WebCryptoAlgorithmParamsType>(
          algorithm_info->operation_to_params_type[op]);

  std::unique_ptr<WebCryptoAlgorithmParams> params;
  if (!ParseAlgorithmParams(isolate, raw, params_type, params, context,
                            exception_state))
    return false;

  algorithm = WebCryptoAlgorithm(*algorithm_id, std::move(params));
  return true;
}

bool ParseAlgorithmIdentifier(v8::Isolate* isolate,
                              const V8AlgorithmIdentifier& raw,
                              WebCryptoOperation op,
                              WebCryptoAlgorithm& algorithm,
                              ErrorContext context,
                              ExceptionState& exception_state) {
  context.Add("Algorithm");

  // If the AlgorithmIdentifier is a String, treat it the same as a Dictionary
  // with a "name" attribute and nothing else.
  if (raw.IsString()) {
    return ParseAlgorithmDictionary(isolate, raw.GetAsString(), Dictionary(),
                                    op, algorithm, context, exception_state);
  }

  // Get the name of the algorithm from the AlgorithmIdentifier.
  Dictionary params(raw.GetAsObject());
  if (exception_state.HadException()) {
    return false;
  }

  std::optional<String> algorithm_name =
      params.Get<IDLString>("name", exception_state);
  if (exception_state.HadException()) {
    return false;
  }

  if (!algorithm_name.has_value()) {
    SetTypeError(context.ToString("name", "Missing or not a string"),
                 exception_state);
    return false;
  }

  return ParseAlgorithmDictionary(isolate, *algorithm_name, params, op,
                                  algorithm, context, exception_state);
}

}  // namespace

bool NormalizeAlgorithm(v8::Isolate* isolate,
                        const V8AlgorithmIdentifier* raw,
                        WebCryptoOperation op,
                        WebCryptoAlgorithm& algorithm,
                        ExceptionState& exception_state) {
  DCHECK(raw);
  return ParseAlgorithmIdentifier(isolate, *raw, op, algorithm, ErrorContext(),
                                  exception_state);
}

}  // namespace blink
