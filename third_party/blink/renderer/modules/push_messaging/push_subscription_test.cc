// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(PushSubscriptionTest, SerializesToBase64URLWithoutPadding) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_testing_scope;

  // Byte value of a p256dh public key with the following base64 encoding:
  //     BLUVyRrO1ZGword7py9iCOCt005VKuFQQ2_ixqM30eTi97Is0_Gqc84O3qCcwb63TOkdY-
  //     7WGnn1dqA3unX60eU=
  Vector<unsigned char> kP256DH(
      {0x04, 0xB5, 0x15, 0xC9, 0x1A, 0xCE, 0xD5, 0x91, 0xB0, 0xA2, 0xB7,
       0x7B, 0xA7, 0x2F, 0x62, 0x08, 0xE0, 0xAD, 0xD3, 0x4E, 0x55, 0x2A,
       0xE1, 0x50, 0x43, 0x6F, 0xE2, 0xC6, 0xA3, 0x37, 0xD1, 0xE4, 0xE2,
       0xF7, 0xB2, 0x2C, 0xD3, 0xF1, 0xAA, 0x73, 0xCE, 0x0E, 0xDE, 0xA0,
       0x9C, 0xC1, 0xBE, 0xB7, 0x4C, 0xE9, 0x1D, 0x63, 0xEE, 0xD6, 0x1A,
       0x79, 0xF5, 0x76, 0xA0, 0x37, 0xBA, 0x75, 0xFA, 0xD1, 0xE5});

  // Byte value of an authentication secret with the following base64 encoding:
  //     6EtIXUjKlyOjRQi9oSly_A==
  Vector<unsigned char> kAuthSecret({0xE8, 0x4B, 0x48, 0x5D, 0x48, 0xCA, 0x97,
                                     0x23, 0xA3, 0x45, 0x08, 0xBD, 0xA1, 0x29,
                                     0x72, 0xFC});

  PushSubscription* subscription = MakeGarbageCollected<PushSubscription>(
      KURL() /* endpoint */, true /* user_visible_only */,
      Vector<uint8_t>() /* application_server_key */, kP256DH, kAuthSecret,
      std::nullopt /* expiration_time */,
      nullptr /* service_worker_registration */);

  ScriptValue json_object =
      subscription->toJSONForBinding(v8_testing_scope.GetScriptState());
  EXPECT_TRUE(json_object.IsObject());

  String json_string = ToBlinkString<String>(
      v8_testing_scope.GetIsolate(),
      v8::JSON::Stringify(v8_testing_scope.GetContext(),
                          json_object.V8Value().As<v8::Object>())
          .ToLocalChecked(),
      kDoNotExternalize);

  // The expected serialized output. Observe the absence of padding.
  constexpr char kExpected[] =
      "{\"endpoint\":\"\",\"expirationTime\":null,\"keys\":{\"p256dh\":"
      "\"BLUVyRrO1ZGword7py9iCOCt005VKuFQQ2_ixqM30eTi97Is0_Gqc84O3qCcwb63TOkdY-"
      "7WGnn1dqA3unX60eU\",\"auth\":\"6EtIXUjKlyOjRQi9oSly_A\"}}";

  EXPECT_EQ(String(kExpected), json_string);
}

}  // namespace blink
