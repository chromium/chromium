// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

using mojom::blink::EmbeddedPermissionControlResult;
using mojom::blink::EmbeddedPermissionRequestDescriptor;
using mojom::blink::EmbeddedPermissionRequestDescriptorPtr;
using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using mojom::blink::PermissionStatus;

namespace {

const char kCameraString[] = "Allow camera";
const char kCameraAllowedString[] = "Camera allowed";
const char kMicrophoneString[] = "Allow microphone";
const char kMicrophoneAllowedString[] = "Microphone allowed";
const char kGeolocationString[] = "Share location";
const char kGeolocationAllowedString[] = "Sharing location allowed";
const char kCameraMicrophoneString[] = "Allow microphone and camera";
const char kCameraMicrophoneAllowedString[] = "Camera and microphone allowed";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    switch (resource_id) {
      case IDS_PERMISSION_REQUEST_CAMERA:
        return kCameraString;
      case IDS_PERMISSION_REQUEST_MICROPHONE:
        return kMicrophoneString;
      case IDS_PERMISSION_REQUEST_GEOLOCATION:
        return kGeolocationString;
      case IDS_PERMISSION_REQUEST_CAMERA_ALLOWED:
        return kCameraAllowedString;
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED:
        return kGeolocationAllowedString;
      case IDS_PERMISSION_REQUEST_MICROPHONE_ALLOWED:
        return kMicrophoneAllowedString;
      case IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE:
        return kCameraMicrophoneString;
      case IDS_PERMISSION_REQUEST_CAMERA_MICROPHONE_ALLOWED:
        return kCameraMicrophoneAllowedString;
      default:
        break;
    }
    return TestingPlatformSupport::QueryLocalizedString(resource_id);
  }
};

}  // namespace

class HTMLPemissionElementTestBase : public PageTestBase {
 protected:
  HTMLPemissionElementTestBase() = default;

 private:
  ScopedPermissionElementForTest scoped_feature_{true};
};

TEST_F(HTMLPemissionElementTestBase, SetTypeAttribute) {
  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("camera"));
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("geolocation"));

  EXPECT_EQ(AtomicString("camera"), permission_element->GetType());
}

TEST_F(HTMLPemissionElementTestBase, ParsePermissionDescriptorsFromType) {
  struct TestData {
    const char* type;
    Vector<PermissionName> expected_permissions;
  } test_data[] = {
      {"camer", {}},
      {"camera", {PermissionName::VIDEO_CAPTURE}},
      {"microphone", {PermissionName::AUDIO_CAPTURE}},
      {"geolocation", {PermissionName::GEOLOCATION}},
      {"camera microphone",
       {PermissionName::VIDEO_CAPTURE, PermissionName::AUDIO_CAPTURE}},
      {" camera     microphone ",
       {PermissionName::VIDEO_CAPTURE, PermissionName::AUDIO_CAPTURE}},
      {"camera   invalid", {}},
      // For MVP, we only support group permissions of camera and microphone
      {"camera microphone geolocation", {}},
      {"camera geolocation", {}},
      {"camera camera", {PermissionName::VIDEO_CAPTURE}},
      {"microphone geolocation", {}},
  };

  for (const auto& data : test_data) {
    Vector<PermissionDescriptorPtr> expected_permission_descriptors;
    expected_permission_descriptors.reserve(data.expected_permissions.size());
    base::ranges::transform(data.expected_permissions,
                            std::back_inserter(expected_permission_descriptors),
                            [&](const auto& name) {
                              auto descriptor = PermissionDescriptor::New();
                              descriptor->name = name;
                              return descriptor;
                            });
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    EXPECT_EQ(expected_permission_descriptors,
              permission_element->ParsePermissionDescriptorsForTesting(
                  permission_element->GetType()));
  }
}

class TestPermissionService : public PermissionService {
 public:
  explicit TestPermissionService(
      mojo::PendingReceiver<PermissionService> pending_receiver)
      : receiver_(this) {
    receiver_.Bind(std::move(pending_receiver));
  }
  ~TestPermissionService() override = default;

  // mojom::blink::PermissionService implementation
  void HasPermission(PermissionDescriptorPtr permission,
                     HasPermissionCallback) override {}
  void RegisterPageEmbeddedPermissionControl(
      Vector<PermissionDescriptorPtr> permissions,
      RegisterPageEmbeddedPermissionControlCallback callback) override {
    Vector<PermissionStatus> statuses =
        initial_statuses_.empty()
            ? Vector<PermissionStatus>(permissions.size(),
                                       PermissionStatus::ASK)
            : initial_statuses_;
    std::move(callback).Run(/*allowed=*/true, std::move(statuses));
  }
  void RequestPageEmbeddedPermission(
      EmbeddedPermissionRequestDescriptorPtr permissions,
      RequestPageEmbeddedPermissionCallback) override {}
  void RequestPermission(PermissionDescriptorPtr permission,
                         bool user_gesture,
                         RequestPermissionCallback) override {}
  void RequestPermissions(Vector<PermissionDescriptorPtr> permissions,
                          bool user_gesture,
                          RequestPermissionsCallback) override {}
  void RevokePermission(PermissionDescriptorPtr permission,
                        RevokePermissionCallback) override {}
  void AddPermissionObserver(
      PermissionDescriptorPtr permission,
      PermissionStatus last_known_status,
      mojo::PendingRemote<PermissionObserver> observer) override {}

  void NotifyEventListener(PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override {}

  void set_initial_statuses(const Vector<PermissionStatus>& statuses) {
    initial_statuses_ = statuses;
  }

 private:
  mojo::Receiver<PermissionService> receiver_;
  HashMap<PermissionName, mojo::Remote<PermissionObserver>> observers_;
  Vector<PermissionStatus> initial_statuses_;
};

class InnerTextChangeWaiter {
 public:
  explicit InnerTextChangeWaiter(HTMLSpanElement* element)
      : element_(element) {}

  InnerTextChangeWaiter(const InnerTextChangeWaiter&) = delete;
  InnerTextChangeWaiter& operator=(const InnerTextChangeWaiter&) = delete;

  void Wait() {
    PostDelayedTask();
    run_loop_.Run();
  }

  void PostDelayedTask() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&InnerTextChangeWaiter::VerifyInnerText,
                      base::Unretained(this)),
        base::Milliseconds(500));
  }
  void VerifyInnerText() {
    if (element_ && element_->innerText().empty()) {
      PostDelayedTask();
    } else {
      run_loop_.Quit();
    }
  }

 private:
  WeakPersistent<HTMLSpanElement> element_;
  base::RunLoop run_loop_;
};

class HTMLPemissionElementTest : public HTMLPemissionElementTestBase {
 protected:
  HTMLPemissionElementTest() = default;

  void SetUp() override {
    HTMLPemissionElementTestBase::SetUp();
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        WTF::BindRepeating(&HTMLPemissionElementTest::Bind,
                           WTF::Unretained(this)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    permission_service_.reset();
    HTMLPemissionElementTestBase::TearDown();
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    permission_service_ = std::make_unique<TestPermissionService>(
        mojo::PendingReceiver<PermissionService>(
            std::move(message_pipe_handle)));
  }

  TestPermissionService* permission_service() {
    return permission_service_.get();
  }

 private:
  std::unique_ptr<TestPermissionService> permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support_;
};

TEST_F(HTMLPemissionElementTest, SetInnerTextAfterRegistrationSingleElement) {
  const struct {
    const char* type;
    PermissionStatus status;
    String expected_text;
  } kTestData[] = {
      {"geolocation", PermissionStatus::ASK, kGeolocationString},
      {"microphone", PermissionStatus::ASK, kMicrophoneString},
      {"camera", PermissionStatus::ASK, kCameraString},
      {"geolocation", PermissionStatus::DENIED, kGeolocationString},
      {"microphone", PermissionStatus::DENIED, kMicrophoneString},
      {"camera", PermissionStatus::DENIED, kCameraString},
      {"geolocation", PermissionStatus::GRANTED, kGeolocationAllowedString},
      {"microphone", PermissionStatus::GRANTED, kMicrophoneAllowedString},
      {"camera", PermissionStatus::GRANTED, kCameraAllowedString}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    permission_service()->set_initial_statuses({data.status});
    InnerTextChangeWaiter waiter(
        permission_element->permission_text_span_for_testing());
    waiter.Wait();
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPemissionElementTest,
       SetInnerTextAfterRegistrationCameraMicrophonePermissions) {
  const struct {
    PermissionStatus camera_status;
    PermissionStatus microphone_status;
    String expected_text;
  } kTestData[] = {
      {PermissionStatus::DENIED, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::DENIED, PermissionStatus::ASK,
       kCameraMicrophoneString},
      {PermissionStatus::DENIED, PermissionStatus::GRANTED, kCameraString},
      {PermissionStatus::ASK, PermissionStatus::ASK, kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::GRANTED, kCameraString},
      {PermissionStatus::ASK, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::ASK, kMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::DENIED, kMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::GRANTED,
       kCameraMicrophoneAllowedString},
  };
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString("camera microphone"));
    permission_service()->set_initial_statuses(
        {data.camera_status, data.microphone_status});
    InnerTextChangeWaiter waiter(
        permission_element->permission_text_span_for_testing());
    waiter.Wait();
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

}  // namespace blink
