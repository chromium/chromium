// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_user_media_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/html/user_media_request_provider.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

using ::testing::_;

const char* kCameraString = "Use camera";
const char* kMicrophoneString = "Use microphone";
const char* kCameraMicrophoneString = "Use microphone and camera";
const char* kCameraAllowedString = "Camera allowed";
const char* kMicrophoneAllowedString = "Microphone allowed";
const char* kCameraMicrophoneAllowedString = "Camera and microphone allowed";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    switch (resource_id) {
      case IDS_PERMISSION_REQUEST_CAMERA:
        return WebString(kCameraString);
      case IDS_PERMISSION_REQUEST_MICROPHONE:
        return WebString(kMicrophoneString);
      case IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE:
        return WebString(kCameraMicrophoneString);
      case IDS_PERMISSION_REQUEST_CAMERA_ALLOWED:
        return WebString(kCameraAllowedString);
      case IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED:
        return WebString(kMicrophoneAllowedString);
      case IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED:
        return WebString(kCameraMicrophoneAllowedString);
      default:
        break;
    }
    return TestingPlatformSupport::QueryLocalizedString(resource_id);
  }
};

class MockUserMediaRequestProvider final
    : public GarbageCollected<MockUserMediaRequestProvider>,
      public UserMediaRequestProvider {
 public:
  explicit MockUserMediaRequestProvider(LocalDOMWindow& window)
      : UserMediaRequestProvider(window) {}

  MOCK_METHOD(void,
              StartRequest,
              (HTMLUserMediaElement*,
               const Vector<mojom::blink::PermissionDescriptorPtr>&),
              (override));

  static MockUserMediaRequestProvider* CreateAndProvideTo(
      LocalDOMWindow& window) {
    auto* provider = MakeGarbageCollected<MockUserMediaRequestProvider>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, provider);
    return provider;
  }
};

class HTMLUserMediaElementTest : public PageTestBase {
 public:
  HTMLUserMediaElementTest() = default;

 protected:
  ScopedTestingPlatformSupport<LocalePlatformSupport> platform_support_;
  ScopedUserMediaElementLegacyForTest legacy_enabled_{true};
};

TEST_F(HTMLUserMediaElementTest, BranchingLogicBasedOnTypeAttribute) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  // Default state (New capability mode)
  EXPECT_FALSE(element->IsLegacyMode());

  // Set type to simulate legacy mode
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  EXPECT_TRUE(element->IsLegacyMode());

  // Remove type to revert to new capability mode
  element->removeAttribute(html_names::kTypeAttr);
  EXPECT_FALSE(element->IsLegacyMode());
}

TEST_F(HTMLUserMediaElementTest, StartRequestOnClick) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();  // Initialize standard mode

  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // If permission is not granted, a click should not trigger a request.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Grant camera permission. Still should not trigger because we need both.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Grant mic permission. Now it should trigger.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::AUDIO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, OnConstraintsSetTriggersRequest) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  // Initialize status to ASK
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // Simulate a click to create a pending request
  element->click();

  // Grant camera permission. Still should not trigger.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Grant mic permission. Now it should trigger.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::AUDIO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, NoRequestWhenNoPermissionGranted) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  // Initialize status to ASK (not granted)
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // A click should NOT trigger a request because permission is not granted.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, DoNotStartRequestTwiceOnClick) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  // Initialize and grant both permissions.
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map);

  // First click: HandleActivation calls StartMediaStreamRequest.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Second click: HandleActivation calls StartMediaStreamRequest again,
  // but it returns early due to the timestamp.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Reset the timestamp as if the request finished.
  element->ResetMediaStreamRequestTime();

  // Third click: Should trigger a new request.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, GrantedText) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  // Case 1: Camera only - No Constraints (Legacy)
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map_camera;
  init_map_camera.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                         mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map_camera);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraAllowedString);

  // Case 2: Standard mode (requires both) - Camera initialized as GRANTED, Mic
  // as ASK (not both, so not allowed text)
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map_camera_only_granted;
  init_map_camera_only_granted.insert(
      mojom::blink::PermissionName::VIDEO_CAPTURE,
      mojom::blink::PermissionStatus::GRANTED);
  init_map_camera_only_granted.insert(
      mojom::blink::PermissionName::AUDIO_CAPTURE,
      mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map_camera_only_granted);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraMicrophoneString);

  // Case 3: Microphone only - No Constraints (Legacy)
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("microphone"));
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map_mic;
  init_map_mic.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                      mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map_mic);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kMicrophoneAllowedString);

  // Case 4: Standard mode - Microphone initialized as GRANTED, Camera as ASK
  // (not allowed text)
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map_mic_only_granted;
  init_map_mic_only_granted.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                                   mojom::blink::PermissionStatus::GRANTED);
  init_map_mic_only_granted.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                   mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map_mic_only_granted);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraMicrophoneString);

  // Case 5: Camera and Microphone - No Constraints (Legacy)
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr,
                        AtomicString("camera microphone"));
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map_both;
  init_map_both.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                       mojom::blink::PermissionStatus::GRANTED);
  init_map_both.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                       mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map_both);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraMicrophoneAllowedString);

  // Case 6: Camera and Microphone - With Constraints (Standard always both, not
  // allowed text even if granted)
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();
  element->OnPermissionStatusInitialized(init_map_both);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraMicrophoneString);
}

TEST_F(HTMLUserMediaElementTest,
       ClickWhilePermissionRequestInProgressFiresError) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  // Initialize status to ASK.
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  EXPECT_EQ(element->error(), nullptr);

  // First click: should trigger permission request.
  element->click();

  // Second click: should fail because the first request is still in progress.
  element->click();

  // It should fire error.
  ASSERT_NE(element->error(), nullptr);
  EXPECT_EQ(element->error()->code(),
            static_cast<uint16_t>(DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(element->error()->message(),
            "A permission request is already in progress.");
}

TEST_F(HTMLUserMediaElementTest, ClickWhenStyleIsInvalidFiresError) {
  GetDocument().domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::CreateFromString("https://example.com"));
  GetDocument()
      .domWindow()
      ->GetSecurityContext()
      .SetSecureContextModeForTesting(SecureContextMode::kSecureContext);

  PermissionElementTestPermissionService permission_service;
  GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_,
      blink::BindRepeating(&PermissionElementTestPermissionService::BindHandle,
                           base::Unretained(&permission_service)));

  MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();  // Initialize before append

  // Initialize status to ASK.
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // Set invalid style.
  element->setAttribute(html_names::kStyleAttr,
                        AtomicString("display: inline;"));
  GetDocument().body()->AppendChild(element);

  // Run lifecycle to trigger style recalc and DidFinishLifecycleUpdate.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Wait for registration to complete.
  WaitForPermissionElementRegistration(element);

  EXPECT_EQ(element->error(), nullptr);

  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::blink::UserActivationNotificationType::kInteraction);

  // Click should fail because style is invalid (and recently attached).
  element->click();

  // It should fire error.
  ASSERT_NE(element->error(), nullptr);
  EXPECT_EQ(element->error()->code(),
            static_cast<uint16_t>(DOMExceptionCode::kInvalidStateError));

  String message = element->error()->message();
  EXPECT_TRUE(
      message.starts_with("The permission element is disabled due to:"));
  EXPECT_TRUE(message.contains("invalid style"));

  // Clean up binder.
  GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_, {});
}

TEST_F(HTMLUserMediaElementTest, UntrustedClickFiresError) {
  // Disable web test mode to force IsRunningWebTest() to return false,
  // so the untrusted event check is not bypassed.
  ScopedWebTestMode web_test_mode(false);

  GetDocument().domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::CreateFromString("https://example.com"));
  GetDocument()
      .domWindow()
      ->GetSecurityContext()
      .SetSecureContextModeForTesting(SecureContextMode::kSecureContext);

  // Do NOT bypass security.
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  EXPECT_EQ(element->error(), nullptr);

  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::blink::UserActivationNotificationType::kInteraction);

  // Click programmatically (untrusted).
  element->click();

  // It should fire error because the click was untrusted.
  ASSERT_NE(element->error(), nullptr);
  EXPECT_EQ(element->error()->code(),
            static_cast<uint16_t>(DOMExceptionCode::kInvalidStateError));
  EXPECT_EQ(element->error()->message(),
            "The permission element activation must be triggered by a user "
            "gesture.");
}

TEST_F(HTMLUserMediaElementTest, LegacyModeDoesNotRequestMediaStream) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  // Set type to enter legacy mode
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  EXPECT_TRUE(element->IsLegacyMode());

  // Initialize permission status
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus> init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // Click it. It should NOT trigger StartRequest (media stream).
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Grant the permission. It should still NOT trigger StartRequest.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, TypeAttributeIgnoredWhenLegacyDisabled) {
  ScopedUserMediaElementLegacyForTest legacy_disabled(false);
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  EXPECT_FALSE(element->IsLegacyMode());

  // Set type, should be ignored.
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  EXPECT_FALSE(element->IsLegacyMode());
  EXPECT_TRUE(element->GetPermissionDescriptors().empty());
}

TEST_F(HTMLUserMediaElementTest, NonSecureContextBlocked) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(false);
  GetDocument().domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::CreateFromString("http://example.com"));
  GetDocument()
      .domWindow()
      ->GetSecurityContext()
      .SetSecureContextModeForTesting(SecureContextMode::kInsecureContext);

  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map);

  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, MissingTransientUserActivationBlocked) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(false);
  GetDocument().domWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::CreateFromString("https://example.com"));
  GetDocument()
      .domWindow()
      ->GetSecurityContext()
      .SetSecureContextModeForTesting(SecureContextMode::kSecureContext);

  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->ApplyDefaultConstraints();

  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map);

  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, DefaultConstraintsNoSetConstraintsClick) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(
          *GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  GetDocument().body()->AppendChild(element);

  // Run pending tasks to let the deferred ApplyDefaultConstraints execute!
  test::RunPendingTasks();

  const auto& descriptors = element->GetPermissionDescriptors();
  ASSERT_EQ(descriptors.size(), 2U);

  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  init_map.insert(mojom::blink::PermissionName::AUDIO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map);

  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

}  // namespace blink
