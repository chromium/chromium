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

// Helper class used to wait until receiving a permission status change event.
class PermissionStatusChangeWaiter : public PermissionObserver {
 public:
  explicit PermissionStatusChangeWaiter(
      mojo::PendingReceiver<PermissionObserver> receiver,
      base::OnceClosure callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {}

  // PermissionObserver override
  void OnPermissionStatusChange(PermissionStatus status) override {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

 private:
  mojo::Receiver<PermissionObserver> receiver_;
  base::OnceClosure callback_;
};

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
      mojo::PendingRemote<PermissionObserver> observer) override {
    auto inserted_result = observers_.insert(
        permission->name,
        mojo::Remote<PermissionObserver>(std::move(observer)));
    CHECK(inserted_result.is_new_entry);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void NotifyEventListener(PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override {}

  void NotifyPermissionStatusChange(PermissionName name,
                                    PermissionStatus status) {
    auto it = observers_.find(name);
    CHECK(it != observers_.end());
    it->value->OnPermissionStatusChange(status);
    WaitForPermissionStatusChange(status);
  }

  void WaitForPermissionStatusChange(PermissionStatus status) {
    mojo::Remote<PermissionObserver> observer;
    base::RunLoop run_loop;
    auto waiter = std::make_unique<PermissionStatusChangeWaiter>(
        observer.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());
    observer->OnPermissionStatusChange(status);
    run_loop.Run();
  }

  void WaitForPermissionObserverAdded() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void set_initial_statuses(const Vector<PermissionStatus>& statuses) {
    initial_statuses_ = statuses;
  }

 private:
  mojo::Receiver<PermissionService> receiver_;
  HashMap<PermissionName, mojo::Remote<PermissionObserver>> observers_;
  std::unique_ptr<base::RunLoop> run_loop_;
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
      {PermissionStatus::DENIED, PermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::ASK, kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::ASK,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::DENIED,
       kCameraMicrophoneString},
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

TEST_F(HTMLPemissionElementTest, StatusChangeSinglePermissionElement) {
  const struct {
    const char* type;
    PermissionName name;
    PermissionStatus status;
    String expected_text;
  } kTestData[] = {{"geolocation", PermissionName::GEOLOCATION,
                    PermissionStatus::ASK, kGeolocationString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    PermissionStatus::ASK, kMicrophoneString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    PermissionStatus::ASK, kCameraString},
                   {"geolocation", PermissionName::GEOLOCATION,
                    PermissionStatus::DENIED, kGeolocationString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    PermissionStatus::DENIED, kMicrophoneString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    PermissionStatus::DENIED, kCameraString},
                   {"geolocation", PermissionName::GEOLOCATION,
                    PermissionStatus::GRANTED, kGeolocationAllowedString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    PermissionStatus::GRANTED, kMicrophoneAllowedString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    PermissionStatus::GRANTED, kCameraAllowedString}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    permission_service()->WaitForPermissionObserverAdded();
    permission_service()->NotifyPermissionStatusChange(data.name, data.status);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPemissionElementTest,
       StatusesChangeCameraMicrophonePermissionsElement) {
  const struct {
    PermissionStatus camera_status;
    PermissionStatus microphone_status;
    String expected_text;
  } kTestData[] = {
      {PermissionStatus::DENIED, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::DENIED, PermissionStatus::ASK,
       kCameraMicrophoneString},
      {PermissionStatus::DENIED, PermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::ASK, kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {PermissionStatus::ASK, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::ASK,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::DENIED,
       kCameraMicrophoneString},
      {PermissionStatus::GRANTED, PermissionStatus::GRANTED,
       kCameraMicrophoneAllowedString},
  };
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString("camera microphone"));
    permission_service()->WaitForPermissionObserverAdded();
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::VIDEO_CAPTURE, data.camera_status);
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::AUDIO_CAPTURE, data.microphone_status);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

}  // namespace blink
