// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permissions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using mojom::blink::PermissionName;

mojom::blink::PermissionDescriptorPtr CreateDescriptor(PermissionName name) {
  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

mojom::blink::PermissionDescriptorPtr CreateMidiDescriptor(bool sysex) {
  auto descriptor = CreateDescriptor(PermissionName::MIDI);
  auto extension = mojom::blink::MidiPermissionDescriptor::New();
  extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::NewMidi(
      std::move(extension));
  return descriptor;
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameNoExtension) {
  auto left = CreateDescriptor(PermissionName::GEOLOCATION);
  auto right = CreateDescriptor(PermissionName::GEOLOCATION);
  EXPECT_TRUE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameSameExtension) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateMidiDescriptor(/*sysex=*/true);
  EXPECT_TRUE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, SameNameDifferentExtension) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateMidiDescriptor(/*sysex=*/false);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, DifferentName) {
  auto left = CreateDescriptor(PermissionName::GEOLOCATION);
  auto right = CreateDescriptor(PermissionName::VIDEO_CAPTURE);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsDescriptorEquivalenceTest, OneWithExtensionOneWithout) {
  auto left = CreateMidiDescriptor(/*sysex=*/true);
  auto right = CreateDescriptor(PermissionName::MIDI);
  EXPECT_FALSE(ArePermissionDescriptorsEquivalentForTesting(*left, *right));
}

TEST(PermissionsTest, RemoteClientDataJsonPermissionDisabled) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationRemoteClientDataJsonForTest feature_enabled(false);
  V8TestingScope scope;
  v8::Local<v8::Object> raw_permission = v8::Object::New(scope.GetIsolate());
  ASSERT_TRUE(
      raw_permission
          ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "name"),
                V8String(scope.GetIsolate(),
                         "publickey-credentials-remote-client-data-json"))
          .FromJust());

  EXPECT_FALSE(ParsePermissionDescriptor(
      scope.GetScriptState(), ScriptValue(scope.GetIsolate(), raw_permission),
      scope.GetExceptionState()));
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PermissionsTest, RemoteClientDataJsonPermissionEnabled) {
  test::TaskEnvironment task_environment;
  ScopedWebAuthenticationRemoteClientDataJsonForTest feature_enabled(true);
  V8TestingScope scope;
  v8::Local<v8::Object> raw_permission = v8::Object::New(scope.GetIsolate());
  ASSERT_TRUE(
      raw_permission
          ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "name"),
                V8String(scope.GetIsolate(),
                         "publickey-credentials-remote-client-data-json"))
          .FromJust());

  mojom::blink::PermissionDescriptorPtr descriptor = ParsePermissionDescriptor(
      scope.GetScriptState(), ScriptValue(scope.GetIsolate(), raw_permission),
      scope.GetExceptionState());
  ASSERT_TRUE(descriptor);
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(descriptor->name,
            PermissionName::PUBLIC_KEY_CREDENTIALS_REMOTE_CLIENT_DATA_JSON);
}

}  // namespace
}  // namespace blink
