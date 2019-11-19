// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_serializer_for_modules.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_deserializer_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_barcode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_face.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_detected_text.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/imagecapture/point_2d.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate_generator.h"
#include "third_party/blink/renderer/modules/shapedetection/landmark.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::UnorderedElementsAre;

namespace blink {
namespace {

v8::Local<v8::Value> RoundTripForModules(v8::Local<v8::Value> value,
                                         V8TestingScope& scope) {
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState& exception_state = scope.GetExceptionState();
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      V8ScriptValueSerializerForModules(
          script_state, V8ScriptValueSerializerForModules::Options())
          .Serialize(value, exception_state);
  DCHECK_EQ(!serialized_script_value, exception_state.HadException());
  EXPECT_TRUE(serialized_script_value);
  if (!serialized_script_value)
    return v8::Local<v8::Value>();
  return V8ScriptValueDeserializerForModules(script_state,
                                             serialized_script_value)
      .Deserialize();
}

// Checks for a DOM exception, including a rethrown one.
testing::AssertionResult HadDOMExceptionInModulesTest(
    const StringView& name,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!exception_state.HadException())
    return testing::AssertionFailure() << "no exception thrown";
  DOMException* dom_exception = V8DOMException::ToImplWithTypeCheck(
      script_state->GetIsolate(), exception_state.GetException());
  if (!dom_exception) {
    return testing::AssertionFailure()
           << "exception thrown was not a DOMException";
  }
  if (dom_exception->name() != name)
    return testing::AssertionFailure() << "was " << dom_exception->name();
  return testing::AssertionSuccess();
}

static const char kEcdsaPrivateKey[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQghHwQ1xYtCoEhFk7r\n"
    "92u3ozy/MFR4I+9FiN8RYv5J96GhRANCAATLfi7OZLD9sIe5UMfMQnHQgAFaQD8h\n"
    "/cy6tB8wXZcixp7bZDp5t0GCDHqAUZT3Sa/NHaCelmmgPp3zW3lszXKP\n"
    "-----END PRIVATE KEY-----\n";

static const char kEcdsaCertificate[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBFjCBvaADAgECAgkApnGS+DzNWkUwCgYIKoZIzj0EAwIwETEPMA0GA1UEAwwG\n"
    "V2ViUlRDMB4XDTE2MDkxNTE4MDcxMloXDTE2MTAxNjE4MDcxMlowETEPMA0GA1UE\n"
    "AwwGV2ViUlRDMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEy34uzmSw/bCHuVDH\n"
    "zEJx0IABWkA/If3MurQfMF2XIsae22Q6ebdBggx6gFGU90mvzR2gnpZpoD6d81t5\n"
    "bM1yjzAKBggqhkjOPQQDAgNIADBFAiBcTOyiexG0QHa5WhJuGtY6FhVZ5GyBMW+7\n"
    "LkH2QmxICwIhAJCujozN3gjIu7NMxSXuTqueuVz58SefCMA7/vj1TgfV\n"
    "-----END CERTIFICATE-----\n";

static const uint8_t kEcdsaCertificateEncoded[] = {
    0xff, 0x09, 0x3f, 0x00, 0x6b, 0xf1, 0x01, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
    0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54,
    0x45, 0x20, 0x4b, 0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0x4d,
    0x49, 0x47, 0x48, 0x41, 0x67, 0x45, 0x41, 0x4d, 0x42, 0x4d, 0x47, 0x42,
    0x79, 0x71, 0x47, 0x53, 0x4d, 0x34, 0x39, 0x41, 0x67, 0x45, 0x47, 0x43,
    0x43, 0x71, 0x47, 0x53, 0x4d, 0x34, 0x39, 0x41, 0x77, 0x45, 0x48, 0x42,
    0x47, 0x30, 0x77, 0x61, 0x77, 0x49, 0x42, 0x41, 0x51, 0x51, 0x67, 0x68,
    0x48, 0x77, 0x51, 0x31, 0x78, 0x59, 0x74, 0x43, 0x6f, 0x45, 0x68, 0x46,
    0x6b, 0x37, 0x72, 0x0a, 0x39, 0x32, 0x75, 0x33, 0x6f, 0x7a, 0x79, 0x2f,
    0x4d, 0x46, 0x52, 0x34, 0x49, 0x2b, 0x39, 0x46, 0x69, 0x4e, 0x38, 0x52,
    0x59, 0x76, 0x35, 0x4a, 0x39, 0x36, 0x47, 0x68, 0x52, 0x41, 0x4e, 0x43,
    0x41, 0x41, 0x54, 0x4c, 0x66, 0x69, 0x37, 0x4f, 0x5a, 0x4c, 0x44, 0x39,
    0x73, 0x49, 0x65, 0x35, 0x55, 0x4d, 0x66, 0x4d, 0x51, 0x6e, 0x48, 0x51,
    0x67, 0x41, 0x46, 0x61, 0x51, 0x44, 0x38, 0x68, 0x0a, 0x2f, 0x63, 0x79,
    0x36, 0x74, 0x42, 0x38, 0x77, 0x58, 0x5a, 0x63, 0x69, 0x78, 0x70, 0x37,
    0x62, 0x5a, 0x44, 0x70, 0x35, 0x74, 0x30, 0x47, 0x43, 0x44, 0x48, 0x71,
    0x41, 0x55, 0x5a, 0x54, 0x33, 0x53, 0x61, 0x2f, 0x4e, 0x48, 0x61, 0x43,
    0x65, 0x6c, 0x6d, 0x6d, 0x67, 0x50, 0x70, 0x33, 0x7a, 0x57, 0x33, 0x6c,
    0x73, 0x7a, 0x58, 0x4b, 0x50, 0x0a, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45,
    0x4e, 0x44, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20, 0x4b,
    0x45, 0x59, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x0a, 0xb4, 0x03, 0x2d, 0x2d,
    0x2d, 0x2d, 0x2d, 0x42, 0x45, 0x47, 0x49, 0x4e, 0x20, 0x43, 0x45, 0x52,
    0x54, 0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d,
    0x2d, 0x0a, 0x4d, 0x49, 0x49, 0x42, 0x46, 0x6a, 0x43, 0x42, 0x76, 0x61,
    0x41, 0x44, 0x41, 0x67, 0x45, 0x43, 0x41, 0x67, 0x6b, 0x41, 0x70, 0x6e,
    0x47, 0x53, 0x2b, 0x44, 0x7a, 0x4e, 0x57, 0x6b, 0x55, 0x77, 0x43, 0x67,
    0x59, 0x49, 0x4b, 0x6f, 0x5a, 0x49, 0x7a, 0x6a, 0x30, 0x45, 0x41, 0x77,
    0x49, 0x77, 0x45, 0x54, 0x45, 0x50, 0x4d, 0x41, 0x30, 0x47, 0x41, 0x31,
    0x55, 0x45, 0x41, 0x77, 0x77, 0x47, 0x0a, 0x56, 0x32, 0x56, 0x69, 0x55,
    0x6c, 0x52, 0x44, 0x4d, 0x42, 0x34, 0x58, 0x44, 0x54, 0x45, 0x32, 0x4d,
    0x44, 0x6b, 0x78, 0x4e, 0x54, 0x45, 0x34, 0x4d, 0x44, 0x63, 0x78, 0x4d,
    0x6c, 0x6f, 0x58, 0x44, 0x54, 0x45, 0x32, 0x4d, 0x54, 0x41, 0x78, 0x4e,
    0x6a, 0x45, 0x34, 0x4d, 0x44, 0x63, 0x78, 0x4d, 0x6c, 0x6f, 0x77, 0x45,
    0x54, 0x45, 0x50, 0x4d, 0x41, 0x30, 0x47, 0x41, 0x31, 0x55, 0x45, 0x0a,
    0x41, 0x77, 0x77, 0x47, 0x56, 0x32, 0x56, 0x69, 0x55, 0x6c, 0x52, 0x44,
    0x4d, 0x46, 0x6b, 0x77, 0x45, 0x77, 0x59, 0x48, 0x4b, 0x6f, 0x5a, 0x49,
    0x7a, 0x6a, 0x30, 0x43, 0x41, 0x51, 0x59, 0x49, 0x4b, 0x6f, 0x5a, 0x49,
    0x7a, 0x6a, 0x30, 0x44, 0x41, 0x51, 0x63, 0x44, 0x51, 0x67, 0x41, 0x45,
    0x79, 0x33, 0x34, 0x75, 0x7a, 0x6d, 0x53, 0x77, 0x2f, 0x62, 0x43, 0x48,
    0x75, 0x56, 0x44, 0x48, 0x0a, 0x7a, 0x45, 0x4a, 0x78, 0x30, 0x49, 0x41,
    0x42, 0x57, 0x6b, 0x41, 0x2f, 0x49, 0x66, 0x33, 0x4d, 0x75, 0x72, 0x51,
    0x66, 0x4d, 0x46, 0x32, 0x58, 0x49, 0x73, 0x61, 0x65, 0x32, 0x32, 0x51,
    0x36, 0x65, 0x62, 0x64, 0x42, 0x67, 0x67, 0x78, 0x36, 0x67, 0x46, 0x47,
    0x55, 0x39, 0x30, 0x6d, 0x76, 0x7a, 0x52, 0x32, 0x67, 0x6e, 0x70, 0x5a,
    0x70, 0x6f, 0x44, 0x36, 0x64, 0x38, 0x31, 0x74, 0x35, 0x0a, 0x62, 0x4d,
    0x31, 0x79, 0x6a, 0x7a, 0x41, 0x4b, 0x42, 0x67, 0x67, 0x71, 0x68, 0x6b,
    0x6a, 0x4f, 0x50, 0x51, 0x51, 0x44, 0x41, 0x67, 0x4e, 0x49, 0x41, 0x44,
    0x42, 0x46, 0x41, 0x69, 0x42, 0x63, 0x54, 0x4f, 0x79, 0x69, 0x65, 0x78,
    0x47, 0x30, 0x51, 0x48, 0x61, 0x35, 0x57, 0x68, 0x4a, 0x75, 0x47, 0x74,
    0x59, 0x36, 0x46, 0x68, 0x56, 0x5a, 0x35, 0x47, 0x79, 0x42, 0x4d, 0x57,
    0x2b, 0x37, 0x0a, 0x4c, 0x6b, 0x48, 0x32, 0x51, 0x6d, 0x78, 0x49, 0x43,
    0x77, 0x49, 0x68, 0x41, 0x4a, 0x43, 0x75, 0x6a, 0x6f, 0x7a, 0x4e, 0x33,
    0x67, 0x6a, 0x49, 0x75, 0x37, 0x4e, 0x4d, 0x78, 0x53, 0x58, 0x75, 0x54,
    0x71, 0x75, 0x65, 0x75, 0x56, 0x7a, 0x35, 0x38, 0x53, 0x65, 0x66, 0x43,
    0x4d, 0x41, 0x37, 0x2f, 0x76, 0x6a, 0x31, 0x54, 0x67, 0x66, 0x56, 0x0a,
    0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x45, 0x4e, 0x44, 0x20, 0x43, 0x45, 0x52,
    0x54, 0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2d, 0x2d, 0x2d, 0x2d,
    0x2d, 0x0a};

TEST(V8ScriptValueSerializerForModulesTest, RoundTripRTCCertificate) {
  // If WebRTC is not supported in this build, this test is meaningless.
  std::unique_ptr<RTCCertificateGenerator> certificate_generator =
      std::make_unique<RTCCertificateGenerator>();
  if (!certificate_generator)
    return;

  V8TestingScope scope;

  // Make a certificate with the existing key above.
  rtc::scoped_refptr<rtc::RTCCertificate> web_certificate =
      certificate_generator->FromPEM(
          WebString::FromUTF8(kEcdsaPrivateKey, sizeof(kEcdsaPrivateKey)),
          WebString::FromUTF8(kEcdsaCertificate, sizeof(kEcdsaCertificate)));
  ASSERT_TRUE(web_certificate);
  RTCCertificate* certificate =
      MakeGarbageCollected<RTCCertificate>(std::move(web_certificate));

  // Round trip test.
  v8::Local<v8::Value> wrapper =
      ToV8(certificate, scope.GetContext()->Global(), scope.GetIsolate());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8RTCCertificate::HasInstance(result, scope.GetIsolate()));
  RTCCertificate* new_certificate =
      V8RTCCertificate::ToImpl(result.As<v8::Object>());
  rtc::RTCCertificatePEM pem = new_certificate->Certificate()->ToPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.private_key());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeRTCCertificate) {
  // If WebRTC is not supported in this build, this test is meaningless.
  std::unique_ptr<RTCCertificateGenerator> certificate_generator =
      std::make_unique<RTCCertificateGenerator>();
  if (!certificate_generator)
    return;

  V8TestingScope scope;

  // This is encoded data generated from Chromium (around M55).
  ScriptState* script_state = scope.GetScriptState();
  Vector<uint8_t> encoded_data;
  encoded_data.Append(kEcdsaCertificateEncoded,
                      sizeof(kEcdsaCertificateEncoded));
  scoped_refptr<SerializedScriptValue> input = SerializedValue(encoded_data);

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8RTCCertificate::HasInstance(result, scope.GetIsolate()));
  RTCCertificate* new_certificate =
      V8RTCCertificate::ToImpl(result.As<v8::Object>());
  rtc::RTCCertificatePEM pem = new_certificate->Certificate()->ToPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.private_key());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidRTCCertificate) {
  V8TestingScope scope;

  // This is valid, except that "private" is not a valid private key PEM and
  // "certificate" is not a valid certificate PEM. This checks what happens if
  // these fail validation inside WebRTC.
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x6b, 0x07, 'p', 'r', 'i', 'v', 'a', 't', 'e',
       0x0b, 'c',  'e',  'r',  't',  'i',  'f', 'i', 'c', 'a', 't', 'e', 0x00});

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  EXPECT_TRUE(result->IsNull());
}

// A bunch of voodoo which allows the asynchronous WebCrypto operations to be
// called synchronously, with the resulting JavaScript values extracted.

using CryptoKeyPair = std::pair<CryptoKey*, CryptoKey*>;

template <typename T>
T ConvertCryptoResult(v8::Isolate*, const ScriptValue&);
template <>
CryptoKey* ConvertCryptoResult<CryptoKey*>(v8::Isolate* isolate,
                                           const ScriptValue& value) {
  return V8CryptoKey::ToImplWithTypeCheck(isolate, value.V8Value());
}
template <>
CryptoKeyPair ConvertCryptoResult<CryptoKeyPair>(v8::Isolate* isolate,
                                                 const ScriptValue& value) {
  NonThrowableExceptionState exception_state;
  Dictionary dictionary(isolate, value.V8Value(), exception_state);
  v8::Local<v8::Value> private_key, public_key;
  EXPECT_TRUE(dictionary.Get("publicKey", public_key));
  EXPECT_TRUE(dictionary.Get("privateKey", private_key));
  return std::make_pair(V8CryptoKey::ToImplWithTypeCheck(isolate, public_key),
                        V8CryptoKey::ToImplWithTypeCheck(isolate, private_key));
}
template <>
DOMException* ConvertCryptoResult<DOMException*>(v8::Isolate* isolate,
                                                 const ScriptValue& value) {
  return V8DOMException::ToImplWithTypeCheck(isolate, value.V8Value());
}
template <>
WebVector<unsigned char> ConvertCryptoResult<WebVector<unsigned char>>(
    v8::Isolate* isolate,
    const ScriptValue& value) {
  WebVector<unsigned char> vector;
  if (DOMArrayBuffer* buffer =
          V8ArrayBuffer::ToImplWithTypeCheck(isolate, value.V8Value())) {
    vector.Assign(reinterpret_cast<const unsigned char*>(buffer->Data()),
                  buffer->ByteLengthAsSizeT());
  }
  return vector;
}
template <>
bool ConvertCryptoResult<bool>(v8::Isolate*, const ScriptValue& value) {
  return value.V8Value()->IsTrue();
}

template <typename T>
class WebCryptoResultAdapter : public ScriptFunction {
 public:
  WebCryptoResultAdapter(ScriptState* script_state,
                         base::RepeatingCallback<void(T)> function)
      : ScriptFunction(script_state), function_(std::move(function)) {}

 private:
  ScriptValue Call(ScriptValue value) final {
    function_.Run(
        ConvertCryptoResult<T>(GetScriptState()->GetIsolate(), value));
    return ScriptValue::From(GetScriptState(), ToV8UndefinedGenerator());
  }

  base::RepeatingCallback<void(T)> function_;
  template <typename U>
  friend WebCryptoResult ToWebCryptoResult(ScriptState*,
                                           base::RepeatingCallback<void(U)>);
};

template <typename T>
WebCryptoResult ToWebCryptoResult(ScriptState* script_state,
                                  base::RepeatingCallback<void(T)> function) {
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state);
  result->Promise().Then(
      (MakeGarbageCollected<WebCryptoResultAdapter<T>>(script_state,
                                                       std::move(function)))
          ->BindToV8Function(),
      (MakeGarbageCollected<WebCryptoResultAdapter<DOMException*>>(
           script_state, WTF::BindRepeating([](DOMException* exception) {
             CHECK(false) << "crypto operation failed";
           })))
          ->BindToV8Function());
  return result->Result();
}

template <typename T, typename PMF, typename... Args>
T SubtleCryptoSync(ScriptState* script_state, PMF func, Args&&... args) {
  T result;
  (Platform::Current()->Crypto()->*func)(
      std::forward<Args>(args)...,
      ToWebCryptoResult(script_state, WTF::BindRepeating(
                                          [](T* out, T result) {
                                            *out = result;
                                            test::ExitRunLoop();
                                          },
                                          WTF::Unretained(&result))),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  test::EnterRunLoop();
  return result;
}

CryptoKey* SyncGenerateKey(ScriptState* script_state,
                           const WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKey*>(script_state, &WebCrypto::GenerateKey,
                                      algorithm, extractable, usages);
}

CryptoKeyPair SyncGenerateKeyPair(ScriptState* script_state,
                                  const WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKeyPair>(script_state, &WebCrypto::GenerateKey,
                                         algorithm, extractable, usages);
}

CryptoKey* SyncImportKey(ScriptState* script_state,
                         WebCryptoKeyFormat format,
                         WebVector<unsigned char> data,
                         const WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKey*>(script_state, &WebCrypto::ImportKey,
                                      format, data, algorithm, extractable,
                                      usages);
}

WebVector<uint8_t> SyncExportKey(ScriptState* script_state,
                                 WebCryptoKeyFormat format,
                                 const WebCryptoKey& key) {
  return SubtleCryptoSync<WebVector<uint8_t>>(
      script_state, &WebCrypto::ExportKey, format, key);
}

WebVector<uint8_t> SyncEncrypt(ScriptState* script_state,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return SubtleCryptoSync<WebVector<uint8_t>>(script_state, &WebCrypto::Encrypt,
                                              algorithm, key, data);
}

WebVector<uint8_t> SyncDecrypt(ScriptState* script_state,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return SubtleCryptoSync<WebVector<uint8_t>>(script_state, &WebCrypto::Decrypt,
                                              algorithm, key, data);
}

WebVector<uint8_t> SyncSign(ScriptState* script_state,
                            const WebCryptoAlgorithm& algorithm,
                            const WebCryptoKey& key,
                            WebVector<unsigned char> message) {
  return SubtleCryptoSync<WebVector<uint8_t>>(script_state, &WebCrypto::Sign,
                                              algorithm, key, message);
}

bool SyncVerifySignature(ScriptState* script_state,
                         const WebCryptoAlgorithm& algorithm,
                         const WebCryptoKey& key,
                         WebVector<unsigned char> signature,
                         WebVector<unsigned char> message) {
  return SubtleCryptoSync<bool>(script_state, &WebCrypto::VerifySignature,
                                algorithm, key, signature, message);
}

WebVector<uint8_t> SyncDeriveBits(ScriptState* script_state,
                                  const WebCryptoAlgorithm& algorithm,
                                  const WebCryptoKey& key,
                                  unsigned length) {
  return SubtleCryptoSync<WebVector<uint8_t>>(
      script_state, &WebCrypto::DeriveBits, algorithm, key, length);
}

// AES-128-CBC uses AES key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyAES) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Generate a 128-bit AES key.
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoAesKeyGenParams(128));
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdAesCbc, std::move(params));
  CryptoKey* key =
      SyncGenerateKey(script_state, algorithm, true,
                      kWebCryptoKeyUsageEncrypt | kWebCryptoKeyUsageDecrypt);

  // Round trip it and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_TRUE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageEncrypt | kWebCryptoKeyUsageDecrypt,
            new_key->Key().Usages());

  // Check that the keys have the same raw representation.
  WebVector<uint8_t> key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatRaw, key->Key());
  WebVector<uint8_t> new_key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatRaw, new_key->Key());
  EXPECT_THAT(new_key_raw, ElementsAreArray(key_raw));

  // Check that one can decrypt data encrypted with the other.
  Vector<unsigned char> iv(16, 0);
  WebCryptoAlgorithm encrypt_algorithm(
      kWebCryptoAlgorithmIdAesCbc, std::make_unique<WebCryptoAesCbcParams>(iv));
  Vector<unsigned char> plaintext{1, 2, 3};
  WebVector<uint8_t> ciphertext =
      SyncEncrypt(script_state, encrypt_algorithm, key->Key(), plaintext);
  WebVector<uint8_t> new_plaintext =
      SyncDecrypt(script_state, encrypt_algorithm, new_key->Key(), ciphertext);
  EXPECT_THAT(new_plaintext, ElementsAre(1, 2, 3));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyAES) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Decode a 128-bit AES key (non-extractable, decrypt only).
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x01, 0x10, 0x04,
                       0x10, 0x7e, 0x25, 0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83,
                       0x70, 0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_FALSE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageDecrypt, new_key->Key().Usages());

  // Check that it can successfully decrypt data.
  Vector<uint8_t> iv(16, 0);
  Vector<uint8_t> ciphertext{0x33, 0x26, 0xe7, 0x64, 0x11, 0x5e, 0xf4, 0x60,
                             0x96, 0x08, 0x11, 0xaf, 0x65, 0x8b, 0x87, 0x04};
  WebCryptoAlgorithm encrypt_algorithm(
      kWebCryptoAlgorithmIdAesCbc, std::make_unique<WebCryptoAesCbcParams>(iv));
  WebVector<uint8_t> plaintext =
      SyncDecrypt(script_state, encrypt_algorithm, new_key->Key(), ciphertext);
  EXPECT_THAT(plaintext, ElementsAre(1, 2, 3));
}

// HMAC-SHA256 uses HMAC key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyHMAC) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Generate an HMAC-SHA256 key.
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  std::unique_ptr<WebCryptoAlgorithmParams> generate_key_params(
      new WebCryptoHmacKeyGenParams(hash, false, 0));
  WebCryptoAlgorithm generate_key_algorithm(kWebCryptoAlgorithmIdHmac,
                                            std::move(generate_key_params));
  CryptoKey* key =
      SyncGenerateKey(script_state, generate_key_algorithm, true,
                      kWebCryptoKeyUsageSign | kWebCryptoKeyUsageVerify);

  // Round trip it and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_TRUE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageSign | kWebCryptoKeyUsageVerify,
            new_key->Key().Usages());

  // Check that the keys have the same raw representation.
  WebVector<uint8_t> key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatRaw, key->Key());
  WebVector<uint8_t> new_key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatRaw, new_key->Key());
  EXPECT_THAT(new_key_raw, ElementsAreArray(key_raw));

  // Check that one can verify a message signed by the other.
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdHmac, nullptr);
  WebVector<uint8_t> signature =
      SyncSign(script_state, algorithm, key->Key(), message);
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm, new_key->Key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyHMAC) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Decode an HMAC-SHA256 key (non-extractable, verify only).
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x02, 0x40, 0x06, 0x10, 0x40, 0xd9,
       0xbd, 0x0e, 0x84, 0x24, 0x3c, 0xb0, 0xbc, 0xee, 0x36, 0x61, 0xdc,
       0xd0, 0xb0, 0xf5, 0x62, 0x09, 0xab, 0x93, 0x8c, 0x21, 0xaf, 0xb7,
       0x66, 0xa9, 0xfc, 0xd2, 0xaa, 0xd8, 0xd4, 0x79, 0xf2, 0x55, 0x3a,
       0xef, 0x46, 0x03, 0xec, 0x64, 0x2f, 0x68, 0xea, 0x9f, 0x9d, 0x1d,
       0xd2, 0x42, 0xd0, 0x13, 0x6c, 0xe0, 0xe1, 0xed, 0x9c, 0x59, 0x46,
       0x85, 0xaf, 0x41, 0xc4, 0x6a, 0x2d, 0x06, 0x7a});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_FALSE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageVerify, new_key->Key().Usages());

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{0x91, 0xc8, 0x54, 0xc3, 0x19, 0x4e, 0xc5, 0x6c,
                            0x2d, 0x18, 0x91, 0x88, 0xd0, 0x56, 0x4d, 0xb6,
                            0x46, 0xc8, 0xb2, 0xa4, 0x2e, 0x1f, 0x0d, 0xe2,
                            0xd6, 0x60, 0xf9, 0xee, 0xb7, 0xd4, 0x55, 0x12};
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdHmac, nullptr);
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm, new_key->Key(),
                                  signature, message));
}

// RSA-PSS-SHA256 uses RSA hashed key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyRSAHashed) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Generate an RSA-PSS-SHA256 key pair.
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  std::unique_ptr<WebCryptoAlgorithmParams> generate_key_params(
      new WebCryptoRsaHashedKeyGenParams(hash, 1024, Vector<uint8_t>{1, 0, 1}));
  WebCryptoAlgorithm generate_key_algorithm(kWebCryptoAlgorithmIdRsaPss,
                                            std::move(generate_key_params));
  CryptoKey* public_key;
  CryptoKey* private_key;
  std::tie(public_key, private_key) =
      SyncGenerateKeyPair(script_state, generate_key_algorithm, true,
                          kWebCryptoKeyUsageSign | kWebCryptoKeyUsageVerify);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(private_key, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_private_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("private", new_private_key->type());
  EXPECT_TRUE(new_private_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageSign, new_private_key->Key().Usages());

  // Check that the keys have the same PKCS8 representation.
  WebVector<uint8_t> key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatPkcs8, private_key->Key());
  WebVector<uint8_t> new_key_raw = SyncExportKey(
      script_state, kWebCryptoKeyFormatPkcs8, new_private_key->Key());
  EXPECT_THAT(new_key_raw, ElementsAreArray(key_raw));

  // Check that one can verify a message signed by the other.
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdRsaPss,
                               std::make_unique<WebCryptoRsaPssParams>(16));
  WebVector<uint8_t> signature =
      SyncSign(script_state, algorithm, new_private_key->Key(), message);
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm, public_key->Key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyRSAHashed) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Decode an RSA-PSS-SHA256 public key (extractable, verify only).
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x04, 0x0d, 0x01, 0x80, 0x08, 0x03, 0x01,
       0x00, 0x01, 0x06, 0x11, 0xa2, 0x01, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06,
       0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
       0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xae,
       0xef, 0x7f, 0xee, 0x3a, 0x48, 0x48, 0xea, 0xce, 0x18, 0x0b, 0x86, 0x34,
       0x6c, 0x1d, 0xc5, 0xe8, 0xea, 0xab, 0x33, 0xd0, 0x6f, 0x63, 0x82, 0x37,
       0x18, 0x83, 0x01, 0x3d, 0x11, 0xe3, 0x03, 0x79, 0x2c, 0x0a, 0x79, 0xe6,
       0xf5, 0x14, 0x73, 0x5f, 0x50, 0xa8, 0x17, 0x10, 0x58, 0x59, 0x20, 0x09,
       0x54, 0x56, 0xe0, 0x86, 0x07, 0x5f, 0xab, 0x9c, 0x86, 0xb1, 0x80, 0xcb,
       0x72, 0x5e, 0x55, 0x8b, 0x83, 0x98, 0xbf, 0xed, 0xbe, 0xdf, 0xdc, 0x6b,
       0xff, 0xcf, 0x50, 0xee, 0xcc, 0x7c, 0xb4, 0x8c, 0x68, 0x75, 0x66, 0xf2,
       0x21, 0x0d, 0xf5, 0x50, 0xdd, 0x06, 0x29, 0x57, 0xf7, 0x44, 0x42, 0x3d,
       0xd9, 0x30, 0xb0, 0x8a, 0x5e, 0x8f, 0xea, 0xff, 0x45, 0xa0, 0x1d, 0x04,
       0xbe, 0xc5, 0x82, 0xd3, 0x69, 0x4e, 0xcd, 0x14, 0x7b, 0xf5, 0x00, 0x3c,
       0xb1, 0x19, 0x24, 0xae, 0x8d, 0x22, 0xb5, 0x02, 0x03, 0x01, 0x00, 0x01});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_public_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("public", new_public_key->type());
  EXPECT_TRUE(new_public_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageVerify, new_public_key->Key().Usages());

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{
      0x9b, 0x61, 0xc8, 0x4b, 0x1c, 0xe5, 0x24, 0xe6, 0x54, 0x73, 0x1a, 0xb5,
      0xe3, 0x22, 0xc7, 0xd1, 0x36, 0x3d, 0x85, 0x99, 0x26, 0x45, 0xcc, 0x54,
      0x98, 0x1f, 0xf3, 0x9d, 0x32, 0x87, 0xdc, 0xbb, 0xb6, 0x3a, 0xa4, 0x6d,
      0xd4, 0xb5, 0x52, 0x83, 0x24, 0x02, 0xc7, 0x62, 0x1f, 0xb7, 0x27, 0x2b,
      0x5a, 0x54, 0x59, 0x17, 0x81, 0x8a, 0xf5, 0x0c, 0x17, 0x01, 0x45, 0x3f,
      0x14, 0xf2, 0x3c, 0x27, 0x4d, 0xfa, 0xc0, 0x0a, 0x82, 0x4b, 0xb2, 0xf4,
      0x7b, 0x14, 0x1b, 0xd8, 0xbc, 0xe9, 0x2e, 0xd4, 0x55, 0x27, 0x62, 0x83,
      0x11, 0xed, 0xc2, 0x81, 0x7d, 0xa9, 0x4f, 0xe0, 0xef, 0x0e, 0xa5, 0xa5,
      0xc6, 0x40, 0x46, 0xbf, 0x90, 0x19, 0xfc, 0xc8, 0x51, 0x0e, 0x0f, 0x62,
      0xeb, 0x17, 0x68, 0x1f, 0xbd, 0xfa, 0xf7, 0xd6, 0x1f, 0xa4, 0x7c, 0x9e,
      0x9e, 0xb1, 0x96, 0x8f, 0xe6, 0x5e, 0x89, 0x99};
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdRsaPss,
                               std::make_unique<WebCryptoRsaPssParams>(16));
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm,
                                  new_public_key->Key(), signature, message));
}

// ECDSA uses EC key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyEC) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Generate an ECDSA key pair with the NIST P-256 curve.
  std::unique_ptr<WebCryptoAlgorithmParams> generate_key_params(
      new WebCryptoEcKeyGenParams(kWebCryptoNamedCurveP256));
  WebCryptoAlgorithm generate_key_algorithm(kWebCryptoAlgorithmIdEcdsa,
                                            std::move(generate_key_params));
  CryptoKey* public_key;
  CryptoKey* private_key;
  std::tie(public_key, private_key) =
      SyncGenerateKeyPair(script_state, generate_key_algorithm, true,
                          kWebCryptoKeyUsageSign | kWebCryptoKeyUsageVerify);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(private_key, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_private_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("private", new_private_key->type());
  EXPECT_TRUE(new_private_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageSign, new_private_key->Key().Usages());

  // Check that the keys have the same PKCS8 representation.
  WebVector<uint8_t> key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatPkcs8, private_key->Key());
  WebVector<uint8_t> new_key_raw = SyncExportKey(
      script_state, kWebCryptoKeyFormatPkcs8, new_private_key->Key());
  EXPECT_THAT(new_key_raw, ElementsAreArray(key_raw));

  // Check that one can verify a message signed by the other.
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  Vector<uint8_t> message{1, 2, 3};
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdEcdsa,
                               std::make_unique<WebCryptoEcdsaParams>(hash));
  WebVector<uint8_t> signature =
      SyncSign(script_state, algorithm, new_private_key->Key(), message);
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm, public_key->Key(),
                                  signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyEC) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Decode an ECDSA public key with the NIST P-256 curve (extractable).
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x01, 0x01, 0x11, 0x5b, 0x30,
       0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
       0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42,
       0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c, 0x11, 0xbf, 0xcf, 0xb7,
       0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66, 0x26, 0xa5, 0xda, 0x69, 0x4c,
       0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4, 0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98,
       0x2a, 0x6f, 0xb2, 0xf5, 0x48, 0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5,
       0x6e, 0x37, 0x0c, 0xfc, 0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4,
       0x0e, 0x1c, 0x31, 0x5a, 0xaa, 0x2d});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_public_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("public", new_public_key->type());
  EXPECT_TRUE(new_public_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageVerify, new_public_key->Key().Usages());

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{1, 2, 3};
  Vector<uint8_t> signature{
      0xee, 0x63, 0xa2, 0xa3, 0x87, 0x6c, 0x9f, 0xc5, 0x64, 0x12, 0x87,
      0x0d, 0xc7, 0xff, 0x3c, 0xd2, 0x6c, 0x2b, 0x2c, 0x0b, 0x2b, 0x8d,
      0x3c, 0xe0, 0x3f, 0xd3, 0xfc, 0x28, 0xf0, 0xa1, 0x22, 0x69, 0x0a,
      0x33, 0x4d, 0x48, 0x97, 0xad, 0x67, 0xa9, 0x6e, 0x24, 0xe7, 0x31,
      0x09, 0xdb, 0xa8, 0x92, 0x48, 0x70, 0xa6, 0x6c, 0x46, 0x4d, 0x0b,
      0x83, 0x27, 0x37, 0x69, 0x4d, 0x32, 0x63, 0x1e, 0x82};
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdEcdsa,
                               std::make_unique<WebCryptoEcdsaParams>(hash));
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm,
                                  new_public_key->Key(), signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyNoParams) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Import some data into a PBKDF2 state.
  WebCryptoAlgorithm import_key_algorithm(kWebCryptoAlgorithmIdPbkdf2, nullptr);
  CryptoKey* key = SyncImportKey(script_state, kWebCryptoKeyFormatRaw,
                                 Vector<uint8_t>{1, 2, 3}, import_key_algorithm,
                                 false, kWebCryptoKeyUsageDeriveBits);

  // Round trip the key and check the visible attributes.
  v8::Local<v8::Value> wrapper = ToV8(key, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_FALSE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageDeriveBits, new_key->Key().Usages());

  // Check that the keys derive the same bits.
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  WebVector<uint8_t> salt(static_cast<size_t>(16));
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoPbkdf2Params(hash, salt, 1));
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdPbkdf2, std::move(params));
  WebVector<uint8_t> bits_raw =
      SyncDeriveBits(script_state, algorithm, key->Key(), 16);
  WebVector<uint8_t> new_bits_raw =
      SyncDeriveBits(script_state, algorithm, new_key->Key(), 16);
  EXPECT_EQ(2u, bits_raw.size());
  EXPECT_THAT(new_bits_raw, ElementsAreArray(bits_raw));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyNoParams) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Decode PBKDF2 state seeded with {1,2,3}.
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x11, 0xa0, 0x02,
                       0x03, 0x01, 0x02, 0x03, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8CryptoKey::HasInstance(result, scope.GetIsolate()));
  CryptoKey* new_key = V8CryptoKey::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("secret", new_key->type());
  EXPECT_FALSE(new_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageDeriveKey | kWebCryptoKeyUsageDeriveBits,
            new_key->Key().Usages());

  // Check that it derives the right bits.
  WebCryptoAlgorithm hash(kWebCryptoAlgorithmIdSha256, nullptr);
  WebVector<uint8_t> salt(static_cast<size_t>(16));
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoPbkdf2Params(hash, salt, 3));
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdPbkdf2, std::move(params));
  WebVector<uint8_t> bits_raw =
      SyncDeriveBits(script_state, algorithm, new_key->Key(), 32);
  EXPECT_THAT(bits_raw, ElementsAre(0xd8, 0x0e, 0x2f, 0x69));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyInvalid) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Invalid algorithm ID.
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  script_state,
                  SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x7f,
                                   0xa0, 0x02, 0x03, 0x01, 0x02, 0x03, 0x00}))
                  .Deserialize()
                  ->IsNull());

  // Algorithm ID / params type mismatch (AES params, RSA-OEAP ID).
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state,
          SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x0a, 0x10, 0x04,
                           0x10, 0x7e, 0x25, 0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83,
                           0x70, 0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7}))
          .Deserialize()
          ->IsNull());

  // Invalid asymmetric key type.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state,
          SerializedValue(
              {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x7f, 0x01, 0x11, 0x5b,
               0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
               0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
               0x07, 0x03, 0x42, 0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c,
               0x11, 0xbf, 0xcf, 0xb7, 0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66,
               0x26, 0xa5, 0xda, 0x69, 0x4c, 0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4,
               0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98, 0x2a, 0x6f, 0xb2, 0xf5, 0x48,
               0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5, 0x6e, 0x37, 0x0c, 0xfc,
               0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4, 0x0e, 0x1c, 0x31,
               0x5a, 0xaa, 0x2d}))
          .Deserialize()
          ->IsNull());

  // Invalid named curve.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state,
          SerializedValue(
              {0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05, 0x0e, 0x01, 0x7f, 0x11, 0x5b,
               0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
               0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01,
               0x07, 0x03, 0x42, 0x00, 0x04, 0xfe, 0x16, 0x70, 0x29, 0x07, 0x2c,
               0x11, 0xbf, 0xcf, 0xb7, 0x9d, 0x54, 0x35, 0x3d, 0xc7, 0x85, 0x66,
               0x26, 0xa5, 0xda, 0x69, 0x4c, 0x07, 0xd5, 0x74, 0xcb, 0x93, 0xf4,
               0xdb, 0x7e, 0x38, 0x3c, 0xa8, 0x98, 0x2a, 0x6f, 0xb2, 0xf5, 0x48,
               0x73, 0x2f, 0x59, 0x21, 0xa0, 0xa9, 0xf5, 0x6e, 0x37, 0x0c, 0xfc,
               0x5b, 0x68, 0x0e, 0x19, 0x5b, 0xd3, 0x4f, 0xb4, 0x0e, 0x1c, 0x31,
               0x5a, 0xaa, 0x2d}))
          .Deserialize()
          ->IsNull());

  // Unknown usage.
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  script_state,
                  SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x11,
                                   0x80, 0x40, 0x03, 0x01, 0x02, 0x03, 0x00}))
                  .Deserialize()
                  ->IsNull());

  // AES key length (16384) that would overflow unsigned short after multiply by
  // 8 (to convert from bytes to bits).
  EXPECT_TRUE(V8ScriptValueDeserializerForModules(
                  script_state,
                  SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x01,
                                   0x80, 0x80, 0x02, 0x04, 0x10, 0x7e, 0x25,
                                   0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83, 0x70,
                                   0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7}))
                  .Deserialize()
                  ->IsNull());

  // HMAC length (1073741824) that would overflow 32-bit unsigned after multiply
  // by 8 (to convert from bytes to bits).
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state,
          SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x02, 0x80, 0x80, 0x80,
                           0x80, 0x04, 0x06, 0x10, 0x40, 0xd9, 0xbd, 0x0e, 0x84,
                           0x24, 0x3c, 0xb0, 0xbc, 0xee, 0x36, 0x61, 0xdc, 0xd0,
                           0xb0, 0xf5, 0x62, 0x09, 0xab, 0x93, 0x8c, 0x21, 0xaf,
                           0xb7, 0x66, 0xa9, 0xfc, 0xd2, 0xaa, 0xd8, 0xd4, 0x79,
                           0xf2, 0x55, 0x3a, 0xef, 0x46, 0x03, 0xec, 0x64, 0x2f,
                           0x68, 0xea, 0x9f, 0x9d, 0x1d, 0xd2, 0x42, 0xd0, 0x13,
                           0x6c, 0xe0, 0xe1, 0xed, 0x9c, 0x59, 0x46, 0x85, 0xaf,
                           0x41, 0xc4, 0x6a, 0x2d, 0x06, 0x7a}))
          .Deserialize()
          ->IsNull());

  // Input ends before end of declared public exponent size.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state, SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x04,
                                         0x0d, 0x01, 0x80, 0x08, 0x03, 0x01}))
          .Deserialize()
          ->IsNull());

  // ECDH key with invalid key data.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state, SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x05,
                                         0x0e, 0x01, 0x01, 0x4b, 0x00, 0x00}))
          .Deserialize()
          ->IsNull());

  // Public RSA key with invalid key data.
  // The key data is a single byte (0x00), which is not a valid SPKI.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state, SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x04,
                                         0x0d, 0x01, 0x80, 0x08, 0x03, 0x01,
                                         0x00, 0x01, 0x06, 0x11, 0x01, 0x00}))
          .Deserialize()
          ->IsNull());
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripDOMFileSystem) {
  V8TestingScope scope;

  auto* fs = MakeGarbageCollected<DOMFileSystem>(
      scope.GetExecutionContext(), "http_example.com_0:Persistent",
      mojom::blink::FileSystemType::kPersistent,
      KURL("filesystem:http://example.com/persistent/"));
  // At time of writing, this can only happen for filesystems from PPAPI.
  fs->MakeClonable();
  v8::Local<v8::Value> wrapper = ToV8(fs, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(V8DOMFileSystem::HasInstance(result, scope.GetIsolate()));
  DOMFileSystem* new_fs = V8DOMFileSystem::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("http_example.com_0:Persistent", new_fs->name());
  EXPECT_EQ(mojom::blink::FileSystemType::kPersistent, new_fs->GetType());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            new_fs->RootURL().GetString());
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripDOMFileSystemNotClonable) {
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");

  auto* fs = MakeGarbageCollected<DOMFileSystem>(
      scope.GetExecutionContext(), "http_example.com_0:Persistent",
      mojom::blink::FileSystemType::kPersistent,
      KURL("filesystem:http://example.com/persistent/0/"));
  ASSERT_FALSE(fs->Clonable());
  v8::Local<v8::Value> wrapper = ToV8(fs, scope.GetScriptState());
  EXPECT_FALSE(V8ScriptValueSerializer(scope.GetScriptState())
                   .Serialize(wrapper, exception_state));
  EXPECT_TRUE(HadDOMExceptionInModulesTest(
      "DataCloneError", scope.GetScriptState(), exception_state));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDOMFileSystem) {
  V8TestingScope scope;

  // This is encoded data generated from Chromium (around M56).
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x64, 0x01, 0x1d, 0x68, 0x74, 0x74, 0x70, 0x5f,
       0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x5f,
       0x30, 0x3a, 0x50, 0x65, 0x72, 0x73, 0x69, 0x73, 0x74, 0x65, 0x6e, 0x74,
       0x29, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x3a,
       0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70,
       0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x70, 0x65, 0x72, 0x73, 0x69,
       0x73, 0x74, 0x65, 0x6e, 0x74, 0x2f});

  // Decode test.
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8DOMFileSystem::HasInstance(result, scope.GetIsolate()));
  DOMFileSystem* new_fs = V8DOMFileSystem::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("http_example.com_0:Persistent", new_fs->name());
  EXPECT_EQ(mojom::blink::FileSystemType::kPersistent, new_fs->GetType());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            new_fs->RootURL().GetString());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidDOMFileSystem) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // Filesystem type out of range.
  EXPECT_TRUE(
      V8ScriptValueDeserializerForModules(
          script_state,
          SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x64, 0x04, 0x1d, 0x68, 0x74,
                           0x74, 0x70, 0x5f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c,
                           0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x5f, 0x30, 0x3a, 0x50,
                           0x65, 0x72, 0x73, 0x69, 0x73, 0x74, 0x65, 0x6e, 0x74,
                           0x29, 0x66, 0x69, 0x6c, 0x65, 0x73, 0x79, 0x73, 0x74,
                           0x65, 0x6d, 0x3a, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f,
                           0x2f, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e,
                           0x63, 0x6f, 0x6d, 0x2f, 0x70, 0x65, 0x72, 0x73, 0x69,
                           0x73, 0x74, 0x65, 0x6e, 0x74, 0x2f

          }))
          .Deserialize()
          ->IsNull());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDetectedBarcode) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x13, 0xff, 0x0d, 0x5c, 'B',  0x04, 0x74, 0x65, 0x78, 0x74, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8DetectedBarcode::HasInstance(result, scope.GetIsolate()));
  DetectedBarcode* detected_barcode =
      V8DetectedBarcode::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("text", detected_barcode->rawValue());
  DOMRectReadOnly* bounding_box = detected_barcode->boundingBox();
  EXPECT_EQ(1, bounding_box->x());
  EXPECT_EQ(2, bounding_box->y());
  EXPECT_EQ(3, bounding_box->width());
  EXPECT_EQ(4, bounding_box->height());
  const HeapVector<Member<Point2D>>& corner_points =
      detected_barcode->cornerPoints();
  EXPECT_EQ(1u, corner_points.size());
  EXPECT_EQ(1, corner_points[0]->x());
  EXPECT_EQ(2, corner_points[0]->y());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDetectedFace) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x13, 0xff, 0x0d, 0x5c, 'F',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x10, 0x40, 0x01, 0x03, 0x65, 0x79, 0x65, 0x01, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8DetectedFace::HasInstance(result, scope.GetIsolate()));
  DetectedFace* detected_face = V8DetectedFace::ToImpl(result.As<v8::Object>());
  DOMRectReadOnly* bounding_box = detected_face->boundingBox();
  EXPECT_EQ(1, bounding_box->x());
  EXPECT_EQ(2, bounding_box->y());
  EXPECT_EQ(3, bounding_box->width());
  EXPECT_EQ(4, bounding_box->height());
  const HeapVector<Member<Landmark>>& landmarks = detected_face->landmarks();
  EXPECT_EQ(1u, landmarks.size());
  EXPECT_EQ("eye", landmarks[0]->type());
  const HeapVector<Member<Point2D>>& locations = landmarks[0]->locations();
  EXPECT_EQ(1u, locations.size());
  EXPECT_EQ(1, locations[0]->x());
  EXPECT_EQ(2, locations[0]->y());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDetectedText) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x13, 0xff, 0x0d, 0x5c, 't',  0x04, 0x74, 0x65, 0x78, 0x74, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  ASSERT_TRUE(V8DetectedText::HasInstance(result, scope.GetIsolate()));
  DetectedText* detected_text = V8DetectedText::ToImpl(result.As<v8::Object>());
  EXPECT_EQ("text", detected_text->rawValue());
  DOMRectReadOnly* bounding_box = detected_text->boundingBox();
  EXPECT_EQ(1, bounding_box->x());
  EXPECT_EQ(2, bounding_box->y());
  EXPECT_EQ(3, bounding_box->width());
  EXPECT_EQ(4, bounding_box->height());
  const HeapVector<Member<Point2D>>& corner_points =
      detected_text->cornerPoints();
  EXPECT_EQ(1u, corner_points.size());
  EXPECT_EQ(1, corner_points[0]->x());
  EXPECT_EQ(2, corner_points[0]->y());
}

}  // namespace
}  // namespace blink
