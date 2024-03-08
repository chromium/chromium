// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
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
using MojoPermissionStatus = mojom::blink::PermissionStatus;

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

void NotReachedForPEPCRegistered() {
  EXPECT_TRUE(false)
      << "The RegisterPageEmbeddedPermissionControl was called despite the "
         "test expecting it not to.";
}

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
  void OnPermissionStatusChange(MojoPermissionStatus status) override {
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
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) override {
    Vector<MojoPermissionStatus> statuses =
        initial_statuses_.empty()
            ? Vector<MojoPermissionStatus>(permissions.size(),
                                           MojoPermissionStatus::ASK)
            : initial_statuses_;
    mojo::Remote<mojom::blink::EmbeddedPermissionControlClient> client(
        std::move(pending_client));
    client->OnEmbeddedPermissionControlRegistered(/*allowed=*/true,
                                                  std::move(statuses));
    if (pepc_registered_callback_) {
      std::move(pepc_registered_callback_).Run();
    }
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
      MojoPermissionStatus last_known_status,
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
                                    MojoPermissionStatus status) {
    auto it = observers_.find(name);
    CHECK(it != observers_.end());
    it->value->OnPermissionStatusChange(status);
    WaitForPermissionStatusChange(status);
  }

  void WaitForPermissionStatusChange(MojoPermissionStatus status) {
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

  void set_initial_statuses(const Vector<MojoPermissionStatus>& statuses) {
    initial_statuses_ = statuses;
  }

  void set_pepc_registered_callback(base::OnceClosure callback) {
    pepc_registered_callback_ = std::move(callback);
  }

 private:
  mojo::Receiver<PermissionService> receiver_;
  HashMap<PermissionName, mojo::Remote<PermissionObserver>> observers_;
  std::unique_ptr<base::RunLoop> run_loop_;
  Vector<MojoPermissionStatus> initial_statuses_;
  base::OnceClosure pepc_registered_callback_;
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

TEST_F(HTMLPemissionElementTest, InitializeInnerText) {
  const struct {
    const char* type;
    String expected_text;
  } kTestData[] = {{"geolocation", kGeolocationString},
                   {"microphone", kMicrophoneString},
                   {"camera", kCameraString},
                   {"camera microphone", kCameraMicrophoneString}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    permission_element->setAttribute(html_names::kStyleAttr,
                                     AtomicString("width: auto; height: auto"));
    GetDocument().body()->AppendChild(permission_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    DOMRect* rect = permission_element->GetBoundingClientRect();
    EXPECT_NE(0, rect->width());
    EXPECT_NE(0, rect->height());
  }
}

TEST_F(HTMLPemissionElementTest, SetInnerTextAfterRegistrationSingleElement) {
  const struct {
    const char* type;
    MojoPermissionStatus status;
    String expected_text;
  } kTestData[] = {
      {"geolocation", MojoPermissionStatus::ASK, kGeolocationString},
      {"microphone", MojoPermissionStatus::ASK, kMicrophoneString},
      {"camera", MojoPermissionStatus::ASK, kCameraString},
      {"geolocation", MojoPermissionStatus::DENIED, kGeolocationString},
      {"microphone", MojoPermissionStatus::DENIED, kMicrophoneString},
      {"camera", MojoPermissionStatus::DENIED, kCameraString},
      {"geolocation", MojoPermissionStatus::GRANTED, kGeolocationAllowedString},
      {"microphone", MojoPermissionStatus::GRANTED, kMicrophoneAllowedString},
      {"camera", MojoPermissionStatus::GRANTED, kCameraAllowedString}};
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
    MojoPermissionStatus camera_status;
    MojoPermissionStatus microphone_status;
    String expected_text;
  } kTestData[] = {
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::GRANTED,
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
    MojoPermissionStatus status;
    String expected_text;
  } kTestData[] = {{"geolocation", PermissionName::GEOLOCATION,
                    MojoPermissionStatus::ASK, kGeolocationString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    MojoPermissionStatus::ASK, kMicrophoneString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    MojoPermissionStatus::ASK, kCameraString},
                   {"geolocation", PermissionName::GEOLOCATION,
                    MojoPermissionStatus::DENIED, kGeolocationString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    MojoPermissionStatus::DENIED, kMicrophoneString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    MojoPermissionStatus::DENIED, kCameraString},
                   {"geolocation", PermissionName::GEOLOCATION,
                    MojoPermissionStatus::GRANTED, kGeolocationAllowedString},
                   {"microphone", PermissionName::AUDIO_CAPTURE,
                    MojoPermissionStatus::GRANTED, kMicrophoneAllowedString},
                   {"camera", PermissionName::VIDEO_CAPTURE,
                    MojoPermissionStatus::GRANTED, kCameraAllowedString}};
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
    MojoPermissionStatus camera_status;
    MojoPermissionStatus microphone_status;
    String expected_text;
  } kTestData[] = {
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::DENIED, MojoPermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::GRANTED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::ASK, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::ASK,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::DENIED,
       kCameraMicrophoneString},
      {MojoPermissionStatus::GRANTED, MojoPermissionStatus::GRANTED,
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

class HTMLPemissionElementSimTest : public SimTest {
 public:
  HTMLPemissionElementSimTest() = default;

  ~HTMLPemissionElementSimTest() override = default;

  void SetUp() override {
    SimTest::SetUp();
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        WTF::BindRepeating(&HTMLPemissionElementSimTest::Bind,
                           WTF::Unretained(this)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    permission_service_.reset();
    SimTest::TearDown();
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
};

TEST_F(HTMLPemissionElementSimTest, BlockedByPermissionsPolicy) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  SimRequest first_iframe_resource("https://example.com/foo1.html",
                                   "text/html");
  SimRequest last_iframe_resource("https://example.com/foo2.html", "text/html");
  main_resource.Complete(R"(
    <body>
      <iframe src='https://example.com/foo1.html'
        allow="camera 'none';microphone 'none';geolocation 'none'">
      </iframe>
      <iframe src='https://example.com/foo2.html'
        allow="camera *;microphone *;geolocation *">
      </iframe>
    </body>
  )");
  first_iframe_resource.Finish();
  last_iframe_resource.Finish();

  auto* first_child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* last_child_frame = To<WebLocalFrameImpl>(MainFrame().LastChild());
  for (const char* permission : {"camera", "microphone", "geolocation"}) {
    auto* permission_element = MakeGarbageCollected<HTMLPermissionElement>(
        *last_child_frame->GetFrame()->GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(permission));
    // PermissionsPolicy passed with no console log.
    auto& last_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            last_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(last_console_messages.size(), 0u);

    permission_element = MakeGarbageCollected<HTMLPermissionElement>(
        *first_child_frame->GetFrame()->GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(permission));
    permission_service()->set_pepc_registered_callback(
        base::BindOnce(&NotReachedForPEPCRegistered));
    // Should console log a error message due to PermissionsPolicy
    auto& first_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            first_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(first_console_messages.size(), 1u);
    EXPECT_TRUE(first_console_messages.front().Contains(
        "is not allowed in the current context due to PermissionsPolicy"));
    first_console_messages.clear();
    permission_service()->set_pepc_registered_callback(base::NullCallback());
  }
}

class HTMLPemissionElementFencedFrameTest : public HTMLPemissionElementSimTest {
 public:
  HTMLPemissionElementFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  ~HTMLPemissionElementFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HTMLPemissionElementFencedFrameTest, NotAllowedInFencedFrame) {
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault);
  SimRequest resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  resource.Complete(R"(
    <body>
    </body>
  )");

  for (const char* permission : {"camera", "microphone", "geolocation"}) {
    auto* permission_element = MakeGarbageCollected<HTMLPermissionElement>(
        *MainFrame().GetFrame()->GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(permission));
    // We need this call to establish binding to the remote permission service,
    // otherwise the next testing binder will fail.
    permission_element->GetPermissionService();
    permission_service()->set_pepc_registered_callback(
        base::BindOnce(&NotReachedForPEPCRegistered));
  }
}

// TODO(crbug.com/1315595): remove this class and use
// `SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME)` once migration
// to blink_unittests_v2 completes. We then can simply use
// `time_environment()->FastForwardBy()`
class ClickingEnabledChecker {
 public:
  explicit ClickingEnabledChecker(HTMLPermissionElement* element)
      : element_(element) {}

  ClickingEnabledChecker(const ClickingEnabledChecker&) = delete;
  ClickingEnabledChecker& operator=(const ClickingEnabledChecker&) = delete;

  void CheckClickingEnabledAfterDelay(base::TimeDelta time,
                                      bool expected_enabled) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&ClickingEnabledChecker::CheckClickingEnabled,
                      base::Unretained(this), expected_enabled),
        time);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void CheckClickingEnabled(bool enabled) {
    EXPECT_EQ(element_->IsClickingEnabled(), enabled);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  Persistent<HTMLPermissionElement> element_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class HTMLPemissionElementIntersectionTest : public SimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLPemissionElementIntersectionTest() = default;

  void SetUp() override {
    SimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    SimTest::TearDown();
  }

  void WaitForFullyVisibleChanged(HTMLPermissionElement* element,
                                  bool fully_visible) {
    // The intersection observer might only detect elements that enter/leave the
    // viewport after a cycle is complete.
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(element->IsFullyVisibleForTesting(), fully_visible);
  }
};

TEST_F(HTMLPemissionElementIntersectionTest, IntersectionChanged) {
  const base::TimeDelta kDefaultTimeout = base::Milliseconds(500);

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 100px;'></div>
    <permission id='camera' type='camera'>
    <div id='trailing' style='height: 700px;'></div>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  WaitForFullyVisibleChanged(permission_element, /*fully_visible*/ true);
  ClickingEnabledChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, kViewportHeight), mojom::blink::ScrollType::kUser);
  WaitForFullyVisibleChanged(permission_element, /*fully_visible*/ false);
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ false);
  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, -kViewportHeight), mojom::blink::ScrollType::kUser);

  // The element is fully visible now but unclickable for a short delay.
  WaitForFullyVisibleChanged(permission_element, /*fully_visible*/ true);
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  EXPECT_TRUE(permission_element->IsFullyVisibleForTesting());
  EXPECT_TRUE(permission_element->IsClickingEnabled());
}

}  // namespace blink
