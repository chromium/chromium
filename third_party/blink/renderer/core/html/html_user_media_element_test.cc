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
#include "third_party/blink/renderer/core/html/user_media_request_provider.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

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

  static MockUserMediaRequestProvider* CreateAndProvideTo(LocalDOMWindow& window) {
    auto* provider = MakeGarbageCollected<MockUserMediaRequestProvider>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, provider);
    return provider;
  }
};

class HTMLUserMediaElementTest : public PageTestBase {
 public:
  HTMLUserMediaElementTest() : platform_support_() {}

 protected:
  ScopedTestingPlatformSupport<LocalePlatformSupport> platform_support_;
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
      MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/false);

  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus> init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE, mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // If permission is not granted, a click should not trigger a request.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);

  // Grant the permission. This automatically calls StartRequest once.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, OnConstraintsSetTriggersRequest) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  // Set constraints instead of 'type'
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/false);

  // Initialize status to ASK
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus> init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE, mojom::blink::PermissionStatus::ASK);
  element->OnPermissionStatusInitialized(init_map);

  // Simulate a click to create a pending request
  element->click();

  // Grant the permission. This should now trigger StartRequest.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(1);
  element->OnPermissionStatusChange(mojom::blink::PermissionName::VIDEO_CAPTURE,
                                    mojom::blink::PermissionStatus::GRANTED);
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, NoRequestWhenNoConstraintsSet) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  // We grant permission, but no constraints are set.
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus> init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
                  mojom::blink::PermissionStatus::GRANTED);
  element->OnPermissionStatusInitialized(init_map);

  // A click should NOT trigger a request because no constraints were set.
  EXPECT_CALL(*provider, StartRequest(element, _)).Times(0);
  element->click();
  ::testing::Mock::VerifyAndClearExpectations(provider);
}

TEST_F(HTMLUserMediaElementTest, NoRequestWhenNoPermissionGranted) {
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc(true);
  MockUserMediaRequestProvider* provider =
      MockUserMediaRequestProvider::CreateAndProvideTo(*GetDocument().domWindow());

  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/false);

  // Initialize status to ASK (not granted)
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus> init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
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
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/false);

  // Initialize and grant the permission.
  HashMap<mojom::blink::PermissionName, mojom::blink::PermissionStatus>
      init_map;
  init_map.insert(mojom::blink::PermissionName::VIDEO_CAPTURE,
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

  // Case 2: Camera only - With Constraints
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/false);
  element->OnPermissionStatusInitialized(init_map_camera);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraString);

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

  // Case 4: Microphone only - With Constraints
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->OnConstraintsSet(/*has_video=*/false, /*has_audio=*/true);
  element->OnPermissionStatusInitialized(init_map_mic);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kMicrophoneString);

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

  // Case 6: Camera and Microphone - With Constraints
  element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());
  element->OnConstraintsSet(/*has_video=*/true, /*has_audio=*/true);
  element->OnPermissionStatusInitialized(init_map_both);
  EXPECT_EQ(element->permission_text_span_for_testing()->innerText(),
            kCameraMicrophoneString);
}

}  // namespace blink
