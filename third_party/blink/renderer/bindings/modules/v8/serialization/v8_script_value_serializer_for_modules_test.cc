// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_serializer_for_modules.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_deserializer_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crop_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crypto_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_dom_file_system.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_restriction_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_certificate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_data_channel.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/crypto/crypto_key.h"
#include "third_party/blink/renderer/modules/crypto/crypto_result_impl.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/restriction_target.h"
#include "third_party/blink/renderer/modules/mediastream/test/transfer_test_utils.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate_generator.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_transfer_list.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/fake_webrtc_data_channel.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data_transfer_list.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_transfer_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::UnorderedElementsAre;

namespace blink {
namespace {

v8::Local<v8::Value> RoundTripForModules(
    v8::Local<v8::Value> value,
    V8TestingScope& scope,
    Transferables* transferables = nullptr) {
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState& exception_state = scope.GetExceptionState();
  V8ScriptValueSerializer::Options serialize_options;
  DCHECK(!transferables || transferables->message_ports.empty());
  serialize_options.transferables = transferables;
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      V8ScriptValueSerializerForModules(script_state, serialize_options)
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
testing::AssertionResult HadDOMExceptionInModulesTest(const StringView& name,
                                                      ScriptState* script_state,
                                                      v8::TryCatch& try_catch) {
  if (!try_catch.HasCaught()) {
    return testing::AssertionFailure() << "no exception thrown";
  }
  DOMException* dom_exception = V8DOMException::ToWrappable(
      script_state->GetIsolate(), try_catch.Exception());
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
  test::TaskEnvironment task_environment;
  // If WebRTC is not supported in this build, this test is meaningless.
  std::unique_ptr<RTCCertificateGenerator> certificate_generator =
      std::make_unique<RTCCertificateGenerator>();
  if (!certificate_generator)
    return;

  V8TestingScope scope;

  // Make a certificate with the existing key above.
  rtc::scoped_refptr<rtc::RTCCertificate> web_certificate =
      certificate_generator->FromPEM(WebString::FromUTF8(kEcdsaPrivateKey),
                                     WebString::FromUTF8(kEcdsaCertificate));
  ASSERT_TRUE(web_certificate);
  RTCCertificate* certificate =
      MakeGarbageCollected<RTCCertificate>(std::move(web_certificate));

  // Round trip test.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<RTCCertificate>::ToV8(scope.GetScriptState(), certificate);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  RTCCertificate* new_certificate =
      V8RTCCertificate::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_certificate, nullptr);
  rtc::RTCCertificatePEM pem = new_certificate->Certificate()->ToPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.private_key());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeRTCCertificate) {
  test::TaskEnvironment task_environment;
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
  RTCCertificate* new_certificate =
      V8RTCCertificate::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_certificate, nullptr);
  rtc::RTCCertificatePEM pem = new_certificate->Certificate()->ToPEM();
  EXPECT_EQ(kEcdsaPrivateKey, pem.private_key());
  EXPECT_EQ(kEcdsaCertificate, pem.certificate());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidRTCCertificate) {
  test::TaskEnvironment task_environment;
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
  return V8CryptoKey::ToWrappable(isolate, value.V8Value());
}
template <>
CryptoKeyPair ConvertCryptoResult<CryptoKeyPair>(v8::Isolate* isolate,
                                                 const ScriptValue& value) {
  NonThrowableExceptionState exception_state;
  Dictionary dictionary(isolate, value.V8Value(), exception_state);
  v8::Local<v8::Value> private_key, public_key;
  EXPECT_TRUE(dictionary.Get("publicKey", public_key));
  EXPECT_TRUE(dictionary.Get("privateKey", private_key));
  return std::make_pair(V8CryptoKey::ToWrappable(isolate, public_key),
                        V8CryptoKey::ToWrappable(isolate, private_key));
}
template <>
DOMException* ConvertCryptoResult<DOMException*>(v8::Isolate* isolate,
                                                 const ScriptValue& value) {
  return V8DOMException::ToWrappable(isolate, value.V8Value());
}
template <>
WebVector<unsigned char> ConvertCryptoResult<WebVector<unsigned char>>(
    v8::Isolate* isolate,
    const ScriptValue& value) {
  WebVector<unsigned char> vector;
  DummyExceptionStateForTesting exception_state;
  if (DOMArrayBuffer* buffer = NativeValueTraits<DOMArrayBuffer>::NativeValue(
          isolate, value.V8Value(), exception_state)) {
    vector.Assign(buffer->ByteSpan());
  }
  return vector;
}
template <>
bool ConvertCryptoResult<bool>(v8::Isolate*, const ScriptValue& value) {
  return value.V8Value()->IsTrue();
}

template <typename T>
class WebCryptoResultAdapter : public ScriptFunction::Callable {
 public:
  explicit WebCryptoResultAdapter(base::RepeatingCallback<void(T)> function)
      : function_(std::move(function)) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    function_.Run(ConvertCryptoResult<T>(script_state->GetIsolate(), value));
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }

 private:
  base::RepeatingCallback<void(T)> function_;
  template <typename U>
  friend WebCryptoResult ToWebCryptoResult(ScriptState*,
                                           base::RepeatingCallback<void(U)>);
};

template <typename IDLType, typename T>
WebCryptoResult ToWebCryptoResult(ScriptState* script_state,
                                  base::RepeatingCallback<void(T)> function) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLType>>(script_state);
  auto* result = MakeGarbageCollected<CryptoResultImpl>(script_state, resolver);
  resolver->Promise().Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<WebCryptoResultAdapter<T>>(std::move(function))),
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<WebCryptoResultAdapter<DOMException*>>(
              WTF::BindRepeating([](DOMException* exception) {
                CHECK(false) << "crypto operation failed";
              }))));
  return result->Result();
}

template <typename T, typename IDLType, typename PMF, typename... Args>
T SubtleCryptoSync(ScriptState* script_state, PMF func, Args&&... args) {
  T result;
  base::RunLoop run_loop;
  (Platform::Current()->Crypto()->*func)(
      std::forward<Args>(args)...,
      ToWebCryptoResult<IDLType>(
          script_state,
          WTF::BindRepeating(
              [](T* out, base::OnceClosure quit_closure, T result) {
                *out = result;
                std::move(quit_closure).Run();
              },
              WTF::Unretained(&result), run_loop.QuitClosure())),
      scheduler::GetSingleThreadTaskRunnerForTesting());
  run_loop.Run();
  return result;
}

CryptoKey* SyncGenerateKey(ScriptState* script_state,
                           const WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKey*, IDLAny>(
      script_state, &WebCrypto::GenerateKey, algorithm, extractable, usages);
}

CryptoKeyPair SyncGenerateKeyPair(ScriptState* script_state,
                                  const WebCryptoAlgorithm& algorithm,
                                  bool extractable,
                                  WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKeyPair, IDLAny>(
      script_state, &WebCrypto::GenerateKey, algorithm, extractable, usages);
}

CryptoKey* SyncImportKey(ScriptState* script_state,
                         WebCryptoKeyFormat format,
                         WebVector<unsigned char> data,
                         const WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         WebCryptoKeyUsageMask usages) {
  return SubtleCryptoSync<CryptoKey*, CryptoKey>(
      script_state, &WebCrypto::ImportKey, format, data, algorithm, extractable,
      usages);
}

WebVector<uint8_t> SyncExportKey(ScriptState* script_state,
                                 WebCryptoKeyFormat format,
                                 const WebCryptoKey& key) {
  return SubtleCryptoSync<WebVector<uint8_t>, IDLAny>(
      script_state, &WebCrypto::ExportKey, format, key);
}

WebVector<uint8_t> SyncEncrypt(ScriptState* script_state,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return SubtleCryptoSync<WebVector<uint8_t>, IDLAny>(
      script_state, &WebCrypto::Encrypt, algorithm, key, data);
}

WebVector<uint8_t> SyncDecrypt(ScriptState* script_state,
                               const WebCryptoAlgorithm& algorithm,
                               const WebCryptoKey& key,
                               WebVector<unsigned char> data) {
  return SubtleCryptoSync<WebVector<uint8_t>, IDLAny>(
      script_state, &WebCrypto::Decrypt, algorithm, key, data);
}

WebVector<uint8_t> SyncSign(ScriptState* script_state,
                            const WebCryptoAlgorithm& algorithm,
                            const WebCryptoKey& key,
                            WebVector<unsigned char> message) {
  return SubtleCryptoSync<WebVector<uint8_t>, IDLAny>(
      script_state, &WebCrypto::Sign, algorithm, key, message);
}

bool SyncVerifySignature(ScriptState* script_state,
                         const WebCryptoAlgorithm& algorithm,
                         const WebCryptoKey& key,
                         WebVector<unsigned char> signature,
                         WebVector<unsigned char> message) {
  return SubtleCryptoSync<bool, IDLAny>(script_state,
                                        &WebCrypto::VerifySignature, algorithm,
                                        key, signature, message);
}

WebVector<uint8_t> SyncDeriveBits(ScriptState* script_state,
                                  const WebCryptoAlgorithm& algorithm,
                                  const WebCryptoKey& key,
                                  unsigned length) {
  return SubtleCryptoSync<WebVector<uint8_t>, DOMArrayBuffer>(
      script_state, &WebCrypto::DeriveBits, algorithm, key, length);
}

// AES-128-CBC uses AES key params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyAES) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Generate a 128-bit AES key.
  std::unique_ptr<WebCryptoAlgorithmParams> params(
      new WebCryptoAesKeyGenParams(128));
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdAesCbc, std::move(params));
  CryptoKey* key =
      SyncGenerateKey(script_state, algorithm, true,
                      kWebCryptoKeyUsageEncrypt | kWebCryptoKeyUsageDecrypt);

  // Round trip it and check the visible attributes.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Decode a 128-bit AES key (non-extractable, decrypt only).
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x01, 0x01, 0x10, 0x04,
                       0x10, 0x7e, 0x25, 0xb2, 0xe8, 0x62, 0x3e, 0xd7, 0x83,
                       0x70, 0xa2, 0xae, 0x98, 0x79, 0x1b, 0xc5, 0xf7});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), private_key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_private_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_private_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  CryptoKey* new_public_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_public_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), private_key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_private_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_private_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  CryptoKey* new_public_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_public_key, nullptr);
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

// Ed25519 uses no params.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyEd25519) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Generate an Ed25519 key pair.
  WebCryptoAlgorithm generate_key_algorithm(kWebCryptoAlgorithmIdEd25519,
                                            nullptr);
  CryptoKey* public_key;
  CryptoKey* private_key;
  std::tie(public_key, private_key) =
      SyncGenerateKeyPair(script_state, generate_key_algorithm, true,
                          kWebCryptoKeyUsageSign | kWebCryptoKeyUsageVerify);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), private_key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_private_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_private_key, nullptr);
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
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdEd25519, nullptr);
  WebVector<uint8_t> signature =
      SyncSign(script_state, algorithm, new_private_key->Key(), message);

  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm, public_key->Key(),
                                  signature, message));
}

// Ed25519 uses no params.
TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyEd25519) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Decode an Ed25519 public key (extractable).
  // TEST 3 from https://www.rfc-editor.org/rfc/rfc8032#section-7.1
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x14, 0xff, 0x0f, 0x5c, 0x4b, 0x07, 0x12, 0x01, 0x11, 0x2c,
      0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21,
      0x00, 0xfc, 0x51, 0xcd, 0x8e, 0x62, 0x18, 0xa1, 0xa3, 0x8d, 0xa4,
      0x7e, 0xd0, 0x02, 0x30, 0xf0, 0x58, 0x08, 0x16, 0xed, 0x13, 0xba,
      0x33, 0x03, 0xac, 0x5d, 0xeb, 0x91, 0x15, 0x48, 0x90, 0x80, 0x25,
  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  CryptoKey* new_public_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_public_key, nullptr);
  EXPECT_EQ("public", new_public_key->type());
  EXPECT_TRUE(new_public_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageVerify, new_public_key->Key().Usages());

  // Check that it can successfully verify a signature.
  Vector<uint8_t> message{0xaf, 0x82};
  Vector<uint8_t> signature{
      0x62, 0x91, 0xd6, 0x57, 0xde, 0xec, 0x24, 0x02, 0x48, 0x27, 0xe6,
      0x9c, 0x3a, 0xbe, 0x01, 0xa3, 0x0c, 0xe5, 0x48, 0xa2, 0x84, 0x74,
      0x3a, 0x44, 0x5e, 0x36, 0x80, 0xd7, 0xdb, 0x5a, 0xc3, 0xac, 0x18,
      0xff, 0x9b, 0x53, 0x8d, 0x16, 0xf2, 0x90, 0xae, 0x67, 0xf7, 0x60,
      0x98, 0x4d, 0xc6, 0x59, 0x4a, 0x7c, 0x15, 0xe9, 0x71, 0x6e, 0xd2,
      0x8d, 0xc0, 0x27, 0xbe, 0xce, 0xea, 0x1e, 0xc4, 0x0a,
  };
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdEd25519, nullptr);
  EXPECT_TRUE(SyncVerifySignature(script_state, algorithm,
                                  new_public_key->Key(), signature, message));
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyX25519) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Generate an X25519 key pair.
  WebCryptoAlgorithm generate_key_algorithm(kWebCryptoAlgorithmIdX25519,
                                            nullptr);
  auto [public_key, private_key] = SyncGenerateKeyPair(
      script_state, generate_key_algorithm, true,
      kWebCryptoKeyUsageDeriveKey | kWebCryptoKeyUsageDeriveBits);

  // Round trip the private key and check the visible attributes.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), private_key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_private_key =
      V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_private_key, nullptr);
  EXPECT_EQ("private", new_private_key->type());
  EXPECT_TRUE(new_private_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageDeriveKey | kWebCryptoKeyUsageDeriveBits,
            new_private_key->Key().Usages());

  // Check that the keys have the same PKCS8 representation.
  WebVector<uint8_t> key_raw =
      SyncExportKey(script_state, kWebCryptoKeyFormatPkcs8, private_key->Key());
  WebVector<uint8_t> new_key_raw = SyncExportKey(
      script_state, kWebCryptoKeyFormatPkcs8, new_private_key->Key());
  EXPECT_THAT(new_key_raw, ElementsAreArray(key_raw));

  // Check that the keys derive the same bits.
  auto params =
      std::make_unique<WebCryptoEcdhKeyDeriveParams>(public_key->Key());
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdX25519, std::move(params));
  WebVector<uint8_t> bits_raw =
      SyncDeriveBits(script_state, algorithm, private_key->Key(), 32);
  WebVector<uint8_t> new_bits_raw =
      SyncDeriveBits(script_state, algorithm, new_private_key->Key(), 32);
  EXPECT_EQ(4u, bits_raw.size());
  EXPECT_THAT(new_bits_raw, ElementsAreArray(bits_raw));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeCryptoKeyX25519) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Decode an X25519 private key (non-extractable).
  // TEST from https://www.rfc-editor.org/rfc/rfc7748#section-6.1
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x14, 0xff, 0x0f, 0x5c, 0x4b, 0x08, 0x13, 0x02, 0x80, 0x02, 0x30,
      0x30, 0x2e, 0x02, 0x01, 0x00, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e,
      0x04, 0x22, 0x04, 0x20, 0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d,
      0x3c, 0x16, 0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45, 0xdf, 0x4c, 0x2f, 0x87,
      0xeb, 0xc0, 0x99, 0x2a, 0xb1, 0x77, 0xfb, 0xa5, 0x1d, 0xb9, 0x2c, 0x2a,
  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  CryptoKey* private_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(private_key, nullptr);
  EXPECT_EQ("private", private_key->type());
  EXPECT_FALSE(private_key->extractable());
  EXPECT_EQ(kWebCryptoKeyUsageDeriveBits, private_key->Key().Usages());

  // Decode an X25519 public key (extractable).
  // TEST from https://www.rfc-editor.org/rfc/rfc7748#section-6.1
  input = SerializedValue({
      0xff, 0x14, 0xff, 0x0f, 0x5c, 0x4b, 0x08, 0x13, 0x01, 0x01, 0x2c,
      0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x03, 0x21,
      0x00, 0xde, 0x9e, 0xdb, 0x7d, 0x7b, 0x7d, 0xc1, 0xb4, 0xd3, 0x5b,
      0x61, 0xc2, 0xec, 0xe4, 0x35, 0x37, 0x3f, 0x83, 0x43, 0xc8, 0x5b,
      0x78, 0x67, 0x4d, 0xad, 0xfc, 0x7e, 0x14, 0x6f, 0x88, 0x2b, 0x4f,
  });
  result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  CryptoKey* public_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(public_key, nullptr);
  EXPECT_EQ("public", public_key->type());
  EXPECT_TRUE(public_key->extractable());
  EXPECT_EQ(0, public_key->Key().Usages());

  // Check that it derives the right bits.
  auto params =
      std::make_unique<WebCryptoEcdhKeyDeriveParams>(public_key->Key());
  WebCryptoAlgorithm algorithm(kWebCryptoAlgorithmIdX25519, std::move(params));
  WebVector<uint8_t> bits_raw =
      SyncDeriveBits(script_state, algorithm, private_key->Key(), 32);
  // Shared secret key.
  // TEST from https://www.rfc-editor.org/rfc/rfc7748#section-6.1
  auto expected_bits = ElementsAre(0x4a, 0x5d, 0x9d, 0x5b);
  EXPECT_THAT(bits_raw, expected_bits);
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripCryptoKeyNoParams) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Import some data into a PBKDF2 state.
  WebCryptoAlgorithm import_key_algorithm(kWebCryptoAlgorithmIdPbkdf2, nullptr);
  CryptoKey* key = SyncImportKey(script_state, kWebCryptoKeyFormatRaw,
                                 Vector<uint8_t>{1, 2, 3}, import_key_algorithm,
                                 false, kWebCryptoKeyUsageDeriveBits);

  // Round trip the key and check the visible attributes.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<CryptoKey>::ToV8(scope.GetScriptState(), key);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
  ScriptState* script_state = scope.GetScriptState();

  // Decode PBKDF2 state seeded with {1,2,3}.
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4b, 0x06, 0x11, 0xa0, 0x02,
                       0x03, 0x01, 0x02, 0x03, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializerForModules(script_state, input).Deserialize();
  CryptoKey* new_key = V8CryptoKey::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_key, nullptr);
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://secure.context/"));
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
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  auto* fs = MakeGarbageCollected<DOMFileSystem>(
      scope.GetExecutionContext(), "http_example.com_0:Persistent",
      mojom::blink::FileSystemType::kPersistent,
      KURL("filesystem:http://example.com/persistent/"));
  // At time of writing, this can only happen for filesystems from PPAPI.
  fs->MakeClonable();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMFileSystem>::ToV8(scope.GetScriptState(), fs);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);
  ASSERT_FALSE(result.IsEmpty());
  DOMFileSystem* new_fs =
      V8DOMFileSystem::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_fs, nullptr);
  EXPECT_EQ("http_example.com_0:Persistent", new_fs->name());
  EXPECT_EQ(mojom::blink::FileSystemType::kPersistent, new_fs->GetType());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            new_fs->RootURL().GetString());
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripDOMFileSystemNotClonable) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  auto* fs = MakeGarbageCollected<DOMFileSystem>(
      scope.GetExecutionContext(), "http_example.com_0:Persistent",
      mojom::blink::FileSystemType::kPersistent,
      KURL("filesystem:http://example.com/persistent/0/"));
  ASSERT_FALSE(fs->Clonable());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMFileSystem>::ToV8(scope.GetScriptState(), fs);
  EXPECT_FALSE(
      V8ScriptValueSerializer(scope.GetScriptState())
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(HadDOMExceptionInModulesTest("DataCloneError",
                                           scope.GetScriptState(), try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeDOMFileSystem) {
  test::TaskEnvironment task_environment;
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
  DOMFileSystem* new_fs =
      V8DOMFileSystem::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_fs, nullptr);
  EXPECT_EQ("http_example.com_0:Persistent", new_fs->name());
  EXPECT_EQ(mojom::blink::FileSystemType::kPersistent, new_fs->GetType());
  EXPECT_EQ("filesystem:http://example.com/persistent/",
            new_fs->RootURL().GetString());
}

TEST(V8ScriptValueSerializerForModulesTest, DecodeInvalidDOMFileSystem) {
  test::TaskEnvironment task_environment;
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

TEST(V8ScriptValueSerializerForModulesTest, RoundTripVideoFrame) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const gfx::Size kFrameSize(600, 480);
  scoped_refptr<media::VideoFrame> media_frame =
      media::VideoFrame::CreateBlackFrame(kFrameSize);

  auto* blink_frame = MakeGarbageCollected<VideoFrame>(
      media_frame, scope.GetExecutionContext());

  // Round trip the frame and make sure the size is the same.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<VideoFrame>::ToV8(scope.GetScriptState(), blink_frame);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);

  VideoFrame* new_frame = V8VideoFrame::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_frame, nullptr);
  EXPECT_EQ(new_frame->frame()->natural_size(), kFrameSize);

  EXPECT_FALSE(media_frame->HasOneRef());

  // Closing |blink_frame| and |new_frame| should remove all references
  // to |media_frame|.
  blink_frame->close();
  EXPECT_FALSE(media_frame->HasOneRef());

  new_frame->close();
  EXPECT_TRUE(media_frame->HasOneRef());
}

TEST(V8ScriptValueSerializerForModulesTest, TransferVideoFrame) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const gfx::Size kFrameSize(600, 480);
  scoped_refptr<media::VideoFrame> media_frame =
      media::VideoFrame::CreateBlackFrame(kFrameSize);

  auto* blink_frame = MakeGarbageCollected<VideoFrame>(
      media_frame, scope.GetExecutionContext());

  // Transfer the frame and make sure the size is the same.
  Transferables transferables;
  VideoFrameTransferList* transfer_list =
      transferables.GetOrCreateTransferList<VideoFrameTransferList>();
  transfer_list->video_frames.push_back(blink_frame);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<VideoFrame>::ToV8(scope.GetScriptState(), blink_frame);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  VideoFrame* new_frame = V8VideoFrame::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_frame, nullptr);
  EXPECT_EQ(new_frame->frame()->natural_size(), kFrameSize);

  EXPECT_FALSE(media_frame->HasOneRef());

  // The transfer should have closed the source frame.
  EXPECT_EQ(blink_frame->frame(), nullptr);

  // Closing |new_frame| should remove all references to |media_frame|.
  new_frame->close();
  EXPECT_TRUE(media_frame->HasOneRef());
}

TEST(V8ScriptValueSerializerForModulesTest, ClosedVideoFrameThrows) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  const gfx::Size kFrameSize(600, 480);
  scoped_refptr<media::VideoFrame> media_frame =
      media::VideoFrame::CreateBlackFrame(kFrameSize);

  // Create and close the frame.
  auto* blink_frame = MakeGarbageCollected<VideoFrame>(
      media_frame, scope.GetExecutionContext());
  blink_frame->close();

  // Serializing the closed frame should throw an error.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<VideoFrame>::ToV8(scope.GetScriptState(), blink_frame);
  EXPECT_FALSE(
      V8ScriptValueSerializer(scope.GetScriptState())
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(HadDOMExceptionInModulesTest("DataCloneError",
                                           scope.GetScriptState(), try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripAudioData) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const unsigned kChannels = 2;
  const unsigned kSampleRate = 8000;
  const unsigned kFrames = 500;
  constexpr base::TimeDelta kTimestamp = base::Milliseconds(314);

  auto audio_bus = media::AudioBus::Create(kChannels, kFrames);

  // Populate each sample with a unique value.
  const unsigned kTotalSamples = (kFrames * kChannels);
  const float kSampleMultiplier = 1.0 / kTotalSamples;
  for (unsigned ch = 0; ch < kChannels; ++ch) {
    float* data = audio_bus->channel(ch);
    for (unsigned i = 0; i < kFrames; ++i)
      data[i] = (i + ch * kFrames) * kSampleMultiplier;
  }

  // Copying the data from an AudioBus instead of creating a media::AudioBuffer
  // directly is acceptable/desirable here, as it's a path often exercised when
  // receiving microphone/WebCam data.
  auto audio_buffer =
      media::AudioBuffer::CopyFrom(kSampleRate, kTimestamp, audio_bus.get());

  auto* audio_data = MakeGarbageCollected<AudioData>(std::move(audio_buffer));

  // Round trip the frame and make sure the size is the same.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<AudioData>::ToV8(scope.GetScriptState(), audio_data);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);

  // The data should have been copied, not transferred.
  EXPECT_TRUE(audio_data->data());

  AudioData* new_data = V8AudioData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_data, nullptr);
  EXPECT_EQ(base::Microseconds(new_data->timestamp()), kTimestamp);
  EXPECT_EQ(new_data->numberOfChannels(), kChannels);
  EXPECT_EQ(new_data->numberOfFrames(), kFrames);
  EXPECT_EQ(new_data->sampleRate(), kSampleRate);

  // Copy out the frames to make sure they haven't been changed during the
  // transfer.
  DOMArrayBuffer* copy_dest = DOMArrayBuffer::Create(kFrames, sizeof(float));
  AllowSharedBufferSource* dest =
      MakeGarbageCollected<AllowSharedBufferSource>(copy_dest);
  AudioDataCopyToOptions* options =
      MakeGarbageCollected<AudioDataCopyToOptions>();

  for (unsigned int ch = 0; ch < kChannels; ++ch) {
    options->setPlaneIndex(ch);
    new_data->copyTo(dest, options, scope.GetExceptionState());
    EXPECT_FALSE(scope.GetExceptionState().HadException());

    float* new_samples = static_cast<float*>(copy_dest->Data());

    for (unsigned int i = 0; i < kFrames; ++i)
      ASSERT_EQ(new_samples[i], (i + ch * kFrames) * kSampleMultiplier);
  }

  // Closing the original |audio_data| should not affect |new_data|.
  audio_data->close();
  EXPECT_TRUE(new_data->data());
}

TEST(V8ScriptValueSerializerForModulesTest, TransferAudioData) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const unsigned kFrames = 500;
  auto audio_buffer = media::AudioBuffer::CreateEmptyBuffer(
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
      /*channel_count=*/2,
      /*sample_rate=*/8000, kFrames, base::Milliseconds(314));

  auto* audio_data = MakeGarbageCollected<AudioData>(audio_buffer);

  // Transfer the frame and make sure the size is the same.
  Transferables transferables;
  AudioDataTransferList* transfer_list =
      transferables.GetOrCreateTransferList<AudioDataTransferList>();
  transfer_list->audio_data_collection.push_back(audio_data);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<AudioData>::ToV8(scope.GetScriptState(), audio_data);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  AudioData* new_data = V8AudioData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_data, nullptr);
  EXPECT_EQ(new_data->numberOfFrames(), kFrames);

  EXPECT_FALSE(audio_buffer->HasOneRef());

  // The transfer should have closed the source data.
  EXPECT_EQ(audio_data->format(), std::nullopt);

  // Closing |new_data| should remove all references to |audio_buffer|.
  new_data->close();
  EXPECT_TRUE(audio_buffer->HasOneRef());
}

TEST(V8ScriptValueSerializerForModulesTest, ClosedAudioDataThrows) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  auto audio_buffer = media::AudioBuffer::CreateEmptyBuffer(
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
      /*channel_count=*/2,
      /*sample_rate=*/8000,
      /*frame_count=*/500, base::Milliseconds(314));

  // Create and close the frame.
  auto* audio_data = MakeGarbageCollected<AudioData>(std::move(audio_buffer));
  audio_data->close();

  // Serializing the closed frame should throw an error.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<AudioData>::ToV8(scope.GetScriptState(), audio_data);
  EXPECT_FALSE(
      V8ScriptValueSerializer(scope.GetScriptState())
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(HadDOMExceptionInModulesTest("DataCloneError",
                                           scope.GetScriptState(), try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest, TransferMediaStreamTrack) {
  test::TaskEnvironment task_environment;
  // This flag is default-off for Android, so we force it on to test this
  // functionality.
  ScopedRegionCaptureForTest region_capture(true);
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  const auto session_id = base::UnguessableToken::Create();
  MediaStreamComponent* component =
      MakeTabCaptureVideoComponentForTest(&scope.GetFrame(), session_id);
  MediaStreamTrack* blink_track =
      MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
          scope.GetExecutionContext(), component,
          MediaStreamSource::ReadyState::kReadyStateMuted,
          /*callback=*/base::DoNothing());
  blink_track->setEnabled(false);

  ScopedMockMediaStreamTrackFromTransferredState mock_impl;

  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  // Transferring should have ended the original track.
  EXPECT_TRUE(blink_track->Ended());

  EXPECT_EQ(V8MediaStreamTrack::ToWrappable(scope.GetIsolate(), result),
            mock_impl.return_value.Get());

  const auto& data = mock_impl.last_argument;
  // The assertions here match the TransferredValues in
  // MediaStreamTrackTransferTest.TabCaptureVideoFromTransferredState. If you
  // change this test, please augment MediaStreamTrackTransferTest to test the
  // new scenario.
  EXPECT_EQ(data.track_impl_subtype,
            BrowserCaptureMediaStreamTrack::GetStaticWrapperTypeInfo());
  EXPECT_EQ(data.session_id, session_id);
  // TODO(crbug.com/1352414): assert correct data.transfer_id
  EXPECT_EQ(data.kind, "video");
  EXPECT_EQ(data.id, "component_id");
  EXPECT_EQ(data.label, "test_name");
  EXPECT_EQ(data.enabled, false);
  EXPECT_EQ(data.muted, true);
  EXPECT_EQ(data.content_hint,
            WebMediaStreamTrack::ContentHintType::kVideoMotion);
  EXPECT_EQ(data.ready_state, MediaStreamSource::ReadyState::kReadyStateLive);
  EXPECT_EQ(data.sub_capture_target_version, std::optional<uint32_t>(0));
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferMediaStreamTrackRegionCaptureDisabled) {
  test::TaskEnvironment task_environment;
  // Test with region capture disabled, since this is the default for Android.
  ScopedRegionCaptureForTest region_capture(false);
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  const auto session_id = base::UnguessableToken::Create();
  MediaStreamComponent* component =
      MakeTabCaptureVideoComponentForTest(&scope.GetFrame(), session_id);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateLive,
      /*callback=*/base::DoNothing());

  ScopedMockMediaStreamTrackFromTransferredState mock_impl;

  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  EXPECT_EQ(V8MediaStreamTrack::ToWrappable(scope.GetIsolate(), result),
            mock_impl.return_value.Get());

  const auto& data = mock_impl.last_argument;
  EXPECT_EQ(data.track_impl_subtype,
            MediaStreamTrack::GetStaticWrapperTypeInfo());
  EXPECT_FALSE(data.sub_capture_target_version.has_value());
}

TEST(V8ScriptValueSerializerForModulesTest, TransferAudioMediaStreamTrack) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const auto session_id = base::UnguessableToken::Create();
  MediaStreamComponent* component =
      MakeTabCaptureAudioComponentForTest(session_id);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  ScopedMockMediaStreamTrackFromTransferredState mock_impl;

  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  // Transferring should have ended the original track.
  EXPECT_TRUE(blink_track->Ended());

  EXPECT_EQ(V8MediaStreamTrack::ToWrappable(scope.GetIsolate(), result),
            mock_impl.return_value.Get());

  const auto& data = mock_impl.last_argument;
  // The assertions here match the TransferredValues in
  // MediaStreamTrackTransferTest.TabCaptureAudioFromTransferredState. If you
  // change this test, please augment MediaStreamTrackTransferTest to test the
  // new scenario.
  EXPECT_EQ(data.track_impl_subtype,
            MediaStreamTrack::GetStaticWrapperTypeInfo());
  EXPECT_EQ(data.session_id, session_id);
  // TODO(crbug.com/1352414): assert correct data.transfer_id
  EXPECT_EQ(data.kind, "audio");
  EXPECT_EQ(data.id, "component_id");
  EXPECT_EQ(data.label, "test_name");
  EXPECT_EQ(data.enabled, true);
  EXPECT_EQ(data.muted, true);
  EXPECT_EQ(data.content_hint,
            WebMediaStreamTrack::ContentHintType::kAudioSpeech);
  EXPECT_EQ(data.ready_state, MediaStreamSource::ReadyState::kReadyStateLive);
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferClonedMediaStreamTrackFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  ScriptState* script_state = scope.GetScriptState();
  MediaStreamComponent* video_component = MakeTabCaptureVideoComponentForTest(
      &scope.GetFrame(), base::UnguessableToken::Create());
  // audio_component cases are disabled due to DCHECKs, see crbug.com/371234481.
  // MediaStreamComponent* audio_component =
  //    MakeTabCaptureAudioComponentForTest(base::UnguessableToken::Create());
  for (MediaStreamComponent* component :
       {video_component, /* audio_component */}) {
    MediaStreamTrack* original_track =
        MakeGarbageCollected<BrowserCaptureMediaStreamTrack>(
            scope.GetExecutionContext(), component,
            MediaStreamSource::ReadyState::kReadyStateMuted,
            /*callback=*/base::DoNothing());
    MediaStreamTrack* cloned_track =
        original_track->clone(scope.GetExecutionContext());
    for (MediaStreamTrack* track : {original_track, cloned_track}) {
      v8::TryCatch try_catch(scope.GetIsolate());
      Transferables transferables;
      transferables.media_stream_tracks.push_back(track);
      v8::Local<v8::Value> wrapper =
          ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), track);
      V8ScriptValueSerializer::Options serialize_options;
      serialize_options.transferables = &transferables;
      EXPECT_FALSE(
          V8ScriptValueSerializerForModules(script_state, serialize_options)
              .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
      EXPECT_TRUE(HadDOMExceptionInModulesTest("DataCloneError", script_state,
                                               try_catch));
    }
  }
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferDeviceCaptureMediaStreamTrackFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  auto mock_source = std::make_unique<MediaStreamVideoCapturerSource>(
      scope.GetFrame().GetTaskRunner(TaskType::kInternalMediaRealTime),
      &scope.GetFrame(),
      MediaStreamVideoCapturerSource::SourceStoppedCallback(),
      std::make_unique<MockVideoCapturerSource>());
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      mock_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamDevice device(mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(base::UnguessableToken::Create());
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeVideo, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kVideoMotion);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  // Transferring MediaStreamTrack should fail for Device Capture type device.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferScreenCaptureMediaStreamTrackFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  auto mock_source = std::make_unique<MediaStreamVideoCapturerSource>(
      scope.GetFrame().GetTaskRunner(TaskType::kInternalMediaRealTime),
      &scope.GetFrame(),
      MediaStreamVideoCapturerSource::SourceStoppedCallback(),
      std::make_unique<MockVideoCapturerSource>());
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      mock_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(base::UnguessableToken::Create());
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::MONITOR,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*initial_zoom_level=*/100);
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeVideo, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kVideoMotion);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  // Transferring MediaStreamTrack should fail for screen captures.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferWindowCaptureMediaStreamTrackFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;

  auto mock_source = std::make_unique<MediaStreamVideoCapturerSource>(
      scope.GetFrame().GetTaskRunner(TaskType::kInternalMediaRealTime),
      &scope.GetFrame(),
      MediaStreamVideoCapturerSource::SourceStoppedCallback(),
      std::make_unique<MockVideoCapturerSource>());
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      mock_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(base::UnguessableToken::Create());
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::WINDOW,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*zoom_level=*/100);
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeVideo, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kVideoMotion);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  // Transferring MediaStreamTrack should fail for window captures.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferClosedMediaStreamTrackFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());

  MediaStreamComponent* component = MakeTabCaptureVideoComponentForTest(
      &scope.GetFrame(), base::UnguessableToken::Create());
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());
  blink_track->stopTrack(scope.GetExecutionContext());
  ASSERT_TRUE(blink_track->Ended());

  // Transferring a closed MediaStreamTrack should throw an error.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferMediaStreamTrackInvalidContentHintFails) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());

  MediaStreamComponent* component = MakeTabCaptureVideoComponentForTest(
      &scope.GetFrame(), base::UnguessableToken::Create());
  component->SetContentHint(
      static_cast<WebMediaStreamTrack::ContentHintType>(666));
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  // Transfer a MediaStreamTrack with an invalid contentHint which should throw
  // an error.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  EXPECT_FALSE(
      V8ScriptValueSerializer(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
  EXPECT_FALSE(blink_track->Ended());
}

TEST(V8ScriptValueSerializerForModulesTest,
     TransferMediaStreamTrackNoSessionIdThrows) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());

  auto mock_source = std::make_unique<MediaStreamVideoCapturerSource>(
      scope.GetFrame().GetTaskRunner(TaskType::kInternalMediaRealTime),
      &scope.GetFrame(),
      MediaStreamVideoCapturerSource::SourceStoppedCallback(),
      std::make_unique<MockVideoCapturerSource>());
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      mock_source.get(),
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      /*enabled=*/true);

  MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::BROWSER,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*zoom_level=*/100);
  mock_source->SetDevice(device);
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "test_id", MediaStreamSource::StreamType::kTypeVideo, "test_name",
      /*remote=*/false, std::move(mock_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>("component_id", source,
                                                     std::move(platform_track));
  component->SetContentHint(WebMediaStreamTrack::ContentHintType::kVideoMotion);
  MediaStreamTrack* blink_track = MakeGarbageCollected<MediaStreamTrackImpl>(
      scope.GetExecutionContext(), component,
      MediaStreamSource::ReadyState::kReadyStateMuted,
      /*callback=*/base::DoNothing());

  // Transfer a MediaStreamTrack with no session id should throw an error.
  Transferables transferables;
  transferables.media_stream_tracks.push_back(blink_track);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MediaStreamTrack>::ToV8(scope.GetScriptState(), blink_track);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(wrapper, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(
      HadDOMExceptionInModulesTest("DataCloneError", script_state, try_catch));
  EXPECT_FALSE(blink_track->Ended());
}

TEST(V8ScriptValueSerializerForModulesTest, TransferRTCDataChannel) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTransferableRTCDataChannelForTest scoped_feature(/*enabled=*/true);

  auto native_channel = FakeWebRTCDataChannel::Create();

  auto* original_channel = MakeGarbageCollected<RTCDataChannel>(
      scope.GetExecutionContext(), native_channel);

  EXPECT_TRUE(original_channel->IsTransferable());
  EXPECT_EQ(native_channel->unregister_call_count(), 0);
  EXPECT_EQ(native_channel->unregister_call_count(), 0);

  // Transfer the frame and make sure the size is the same.
  Transferables transferables;
  RTCDataChannelTransferList* transfer_list =
      transferables.GetOrCreateTransferList<RTCDataChannelTransferList>();
  transfer_list->data_channel_collection.push_back(original_channel);
  v8::Local<v8::Value> wrapper = ToV8Traits<RTCDataChannel>::ToV8(
      scope.GetScriptState(), original_channel);
  v8::Local<v8::Value> result =
      RoundTripForModules(wrapper, scope, &transferables);

  RTCDataChannel* new_channel =
      V8RTCDataChannel::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_channel, nullptr);

  // An RTCDataChannel is "neutered" after a single transfer, and cannot be
  // transferred again. However, the new RTCDataChannel can also be transferred
  // once. This allows chaining of transfers of the underlying `native_channel`.
  EXPECT_FALSE(original_channel->IsTransferable());
  EXPECT_TRUE(new_channel->IsTransferable());

  // The transfer should have closed the original channel but not the underlying
  // transport.
  EXPECT_EQ(original_channel->readyState(), "closed");
  EXPECT_FALSE(native_channel->close_was_called());
  EXPECT_EQ(native_channel->unregister_call_count(), 0);

  // The new channel should not have immediately registered its observer. This
  // gives the new RTCDataChannel a brief opportunity to be transferred again;
  // transferring the underlying `native_channel` is allowed until we call
  // `send()`, or register an observer (after which we could lose incoming
  // messages during a transfer).
  EXPECT_EQ(native_channel->register_call_count(), 0);

  task_environment.RunUntilIdle();

  EXPECT_FALSE(new_channel->IsTransferable());

  EXPECT_EQ(native_channel->register_call_count(), 1);
  EXPECT_EQ(native_channel->unregister_call_count(), 0);
  EXPECT_FALSE(native_channel->close_was_called());
}

#if !BUILDFLAG(IS_ANDROID)  // SubCaptureTargets are not exposed on Android.
TEST(V8ScriptValueSerializerForModulesTest, RoundTripCropTarget) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const String crop_id("8e7e0c22-67a0-4c39-b4dc-a20433262f8e");

  CropTarget* const crop_target = MakeGarbageCollected<CropTarget>(crop_id);

  v8::Local<v8::Value> wrapper =
      ToV8Traits<CropTarget>::ToV8(scope.GetScriptState(), crop_target);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);

  CropTarget* const new_crop_target =
      V8CropTarget::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_crop_target, nullptr);
  EXPECT_EQ(new_crop_target->GetId(), crop_id);
}

TEST(V8ScriptValueSerializerForModulesTest, RoundTripRestrictionTarget) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedElementCaptureForTest element_capture(true);

  const String restriction_id("8e7e0c22-67a0-4c39-b4dc-a20433262f8e");

  RestrictionTarget* const restriction_target =
      MakeGarbageCollected<RestrictionTarget>(restriction_id);

  v8::Local<v8::Value> wrapper = ToV8Traits<RestrictionTarget>::ToV8(
      scope.GetScriptState(), restriction_target);
  v8::Local<v8::Value> result = RoundTripForModules(wrapper, scope);

  RestrictionTarget* const new_restriction_target =
      V8RestrictionTarget::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_restriction_target, nullptr);
  EXPECT_EQ(new_restriction_target->GetId(), restriction_id);
}
#endif

TEST(V8ScriptValueSerializerForModulesTest,
     ArrayBufferDetachKeyPreventsTransfer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  DOMArrayBuffer* ab = DOMArrayBuffer::Create(10, sizeof(float));
  v8::Local<v8::ArrayBuffer> v8_ab =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, ab)
          .As<v8::ArrayBuffer>();
  v8_ab->SetDetachKey(V8AtomicString(isolate, "my key"));

  // Attempt to transfer the ArrayBuffer. It should fail with a TypeError
  // because the ArrayBufferDetachKey used to transfer is not "my key".
  Transferables transferables;
  transferables.array_buffers.push_back(ab);
  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  v8::TryCatch try_catch(isolate);
  EXPECT_FALSE(
      V8ScriptValueSerializerForModules(script_state, serialize_options)
          .Serialize(v8_ab, PassThroughException(scope.GetIsolate())));
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_THAT(
      ToCoreString(
          isolate,
          try_catch.Exception()->ToString(scope.GetContext()).ToLocalChecked())
          .Ascii(),
      testing::StartsWith("TypeError"));
  EXPECT_FALSE(v8_ab->WasDetached());
}

TEST(V8ScriptValueSerializerForModulesTest,
     ArrayBufferDetachKeyDoesNotPreventSerialize) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();

  DOMArrayBuffer* ab = DOMArrayBuffer::Create(10, sizeof(float));
  v8::Local<v8::ArrayBuffer> v8_ab =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, ab)
          .As<v8::ArrayBuffer>();
  v8_ab->SetDetachKey(V8AtomicString(isolate, "my key"));

  // Attempt to serialize the ArrayBuffer. It should not fail with a TypeError
  // even though it has an ArrayBufferDetachKey because it will not be detached.
  V8ScriptValueSerializer::Options serialize_options;
  ExceptionState exception_state(isolate, v8::ExceptionContext::kOperation,
                                 "Window", "postMessage");
  EXPECT_TRUE(V8ScriptValueSerializerForModules(script_state, serialize_options)
                  .Serialize(v8_ab, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_FALSE(v8_ab->WasDetached());
}

}  // namespace
}  // namespace blink
