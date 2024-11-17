// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include <optional>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_generated_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_permission_state.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

constexpr char16_t kGeolocationStringPt[] = u"Usar localização";
constexpr char16_t kGeolocationAllowedStringPt[] =
    u"Acesso à localização permitido";
constexpr char16_t kGeolocationStringBr[] = u"Usar local";
constexpr char16_t kGeolocationAllowedStringBr[] =
    u"Acesso à localização permitido";
constexpr char16_t kGeolocationStringTa[] = u"இருப்பிடத்தைப் பயன்படுத்து";
constexpr char16_t kGeolocationAllowedStringTa[] = u"இருப்பிட அனுமதி வழங்கப்பட்டது";

constexpr char kCameraString[] = "Use camera";
constexpr char kCameraAllowedString[] = "Camera allowed";
constexpr char kMicrophoneString[] = "Use microphone";
constexpr char kMicrophoneAllowedString[] = "Microphone allowed";
constexpr char kGeolocationString[] = "Use location";
constexpr char kGeolocationAllowedString[] = "Location allowed";
constexpr char kCameraMicrophoneString[] = "Use microphone and camera";
constexpr char kCameraMicrophoneAllowedString[] =
    "Camera and microphone allowed";
constexpr char kPreciseGeolocationString[] = "Use precise location";
constexpr char kPreciseGeolocationAllowedString[] = "Precise location allowed";

constexpr base::TimeDelta kDefaultTimeout = base::Milliseconds(500);
constexpr base::TimeDelta kSmallTimeout = base::Milliseconds(50);

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
      case IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION:
        return kPreciseGeolocationString;
      case IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION_ALLOWED:
        return kPreciseGeolocationAllowedString;
      case IDS_PERMISSION_REQUEST_GEOLOCATION_pt_PT:
        return WebString::FromUTF16(kGeolocationStringPt);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED_pt_PT:
        return WebString::FromUTF16(kGeolocationAllowedStringPt);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_pt_BR:
        return WebString::FromUTF16(kGeolocationStringBr);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED_pt_BR:
        return WebString::FromUTF16(kGeolocationAllowedStringBr);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ta:
        return WebString::FromUTF16(kGeolocationStringTa);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ALLOWED_ta:
        return WebString::FromUTF16(kGeolocationAllowedStringTa);
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

V8PermissionState::Enum PermissionStatusV8Enum(MojoPermissionStatus status) {
  switch (status) {
    case MojoPermissionStatus::GRANTED:
      return V8PermissionState::Enum::kGranted;
    case MojoPermissionStatus::ASK:
      return V8PermissionState::Enum::kPrompt;
    case MojoPermissionStatus::DENIED:
      return V8PermissionState::Enum::kDenied;
  }
}

}  // namespace

class HTMLPemissionElementTestBase : public PageTestBase {
 protected:
  HTMLPemissionElementTestBase() = default;

  HTMLPemissionElementTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : PageTestBase(time_source) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kPermissionElement);
    PageTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

TEST_F(HTMLPemissionElementTestBase, SetPreciseLocationAttribute) {
  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(GetDocument());

  EXPECT_FALSE(permission_element->is_precise_location_);

  permission_element->setAttribute(html_names::kPreciselocationAttr,
                                   AtomicString(""));
  EXPECT_TRUE(permission_element->is_precise_location_);

  permission_element->removeAttribute(html_names::kPreciselocationAttr);
  EXPECT_TRUE(permission_element->is_precise_location_);
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
  explicit TestPermissionService() = default;
  ~TestPermissionService() override = default;

  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this,
                   mojo::PendingReceiver<PermissionService>(std::move(handle)));
  }

  // mojom::blink::PermissionService implementation
  void HasPermission(PermissionDescriptorPtr permission,
                     HasPermissionCallback) override {}
  void RegisterPageEmbeddedPermissionControl(
      Vector<PermissionDescriptorPtr> permissions,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) override {
    if (pepc_registered_callback_) {
      std::move(pepc_registered_callback_).Run();
      return;
    }

    if (should_defer_registered_callback_) {
      pepc_registered_callback_ = WTF::BindOnce(
          &TestPermissionService::RegisterPageEmbeddedPermissionControlInternal,
          base::Unretained(this), std::move(permissions),
          std::move(pending_client));
      return;
    }

    RegisterPageEmbeddedPermissionControlInternal(std::move(permissions),
                                                  std::move(pending_client));
  }

  void RegisterPageEmbeddedPermissionControlInternal(
      Vector<PermissionDescriptorPtr> permissions,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) {
    Vector<MojoPermissionStatus> statuses =
        initial_statuses_.empty()
            ? Vector<MojoPermissionStatus>(permissions.size(),
                                           MojoPermissionStatus::ASK)
            : initial_statuses_;
    client_ = mojo::Remote<mojom::blink::EmbeddedPermissionControlClient>(
        std::move(pending_client));
    client_.set_disconnect_handler(base::BindOnce(
        &TestPermissionService::OnMojoDisconnect, base::Unretained(this)));
    client_->OnEmbeddedPermissionControlRegistered(/*allowed=*/true,
                                                   std::move(statuses));
  }

  void OnMojoDisconnect() {
    if (client_disconnect_run_loop_) {
      client_disconnect_run_loop_->Quit();
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
      mojo::PendingRemote<PermissionObserver> observer) override {}
  void AddPageEmbeddedPermissionObserver(
      PermissionDescriptorPtr permission,
      MojoPermissionStatus last_known_status,
      mojo::PendingRemote<PermissionObserver> observer) override {
    observers_.emplace_back(permission->name, mojo::Remote<PermissionObserver>(
                                                  std::move(observer)));
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void NotifyEventListener(PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override {}

  void NotifyPermissionStatusChange(PermissionName name,
                                    MojoPermissionStatus status) {
    for (const auto& observer : observers_) {
      if (observer.first == name) {
        observer.second->OnPermissionStatusChange(status);
      }
    }
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

  void WaitForClientDisconnected() {
    client_disconnect_run_loop_ = std::make_unique<base::RunLoop>();
    client_disconnect_run_loop_->Run();
  }

  void set_initial_statuses(const Vector<MojoPermissionStatus>& statuses) {
    initial_statuses_ = statuses;
  }

  void set_pepc_registered_callback(base::OnceClosure callback) {
    pepc_registered_callback_ = std::move(callback);
  }

  base::OnceClosure TakePEPCRegisteredCallback() {
    return std::move(pepc_registered_callback_);
  }

  void set_should_defer_registered_callback(bool should_defer) {
    should_defer_registered_callback_ = should_defer;
  }

 private:
  mojo::ReceiverSet<PermissionService> receivers_;
  Vector<std::pair<PermissionName, mojo::Remote<PermissionObserver>>>
      observers_;
  std::unique_ptr<base::RunLoop> run_loop_;
  Vector<MojoPermissionStatus> initial_statuses_;
  bool should_defer_registered_callback_ = false;
  base::OnceClosure pepc_registered_callback_;
  mojo::Remote<mojom::blink::EmbeddedPermissionControlClient> client_;
  std::unique_ptr<base::RunLoop> client_disconnect_run_loop_;
};

class RegistrationWaiter {
 public:
  explicit RegistrationWaiter(HTMLPermissionElement* element)
      : element_(element) {}

  RegistrationWaiter(const RegistrationWaiter&) = delete;
  RegistrationWaiter& operator=(const RegistrationWaiter&) = delete;

  void Wait() {
    PostDelayedTask();
    run_loop_.Run();
  }

  void PostDelayedTask() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&RegistrationWaiter::VerifyRegistration,
                      base::Unretained(this)),
        base::Milliseconds(500));
  }
  void VerifyRegistration() {
    if (element_ && !element_->IsRegisteredInBrowserProcess()) {
      PostDelayedTask();
    } else {
      run_loop_.Quit();
    }
  }

 private:
  WeakPersistent<HTMLPermissionElement> element_;
  base::RunLoop run_loop_;
};

class HTMLPemissionElementTest : public HTMLPemissionElementTestBase {
 protected:
  HTMLPemissionElementTest() = default;

  HTMLPemissionElementTest(base::test::TaskEnvironment::TimeSource time_source)
      : HTMLPemissionElementTestBase(time_source) {}

  void SetUp() override {
    HTMLPemissionElementTestBase::SetUp();
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        base::BindRepeating(&TestPermissionService::BindHandle,
                            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    HTMLPemissionElementTestBase::TearDown();
  }

  TestPermissionService* permission_service() { return &permission_service_; }

  HTMLPermissionElement* CreatePermissionElement(
      const char* permission,
      bool precise_location = false) {
    HTMLPermissionElement* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(permission));
    if (precise_location) {
      permission_element->setAttribute(html_names::kPreciselocationAttr,
                                       AtomicString(""));
    }
    GetDocument().body()->AppendChild(permission_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    return permission_element;
  }

 private:
  TestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support_;
};

// TODO(crbug.com/1315595): remove this class and use
// `SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME)` once migration
// to blink_unittests_v2 completes. We then can simply use
// `time_environment()->FastForwardBy()`
class DeferredChecker {
 public:
  explicit DeferredChecker(HTMLPermissionElement* element,
                           WebLocalFrameImpl* main_frame = nullptr)
      : element_(element), main_frame_(main_frame) {}

  DeferredChecker(const DeferredChecker&) = delete;
  DeferredChecker& operator=(const DeferredChecker&) = delete;

  void CheckClickingEnabledAfterDelay(base::TimeDelta time,
                                      bool expected_enabled) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&DeferredChecker::CheckClickingEnabled,
                      base::Unretained(this), expected_enabled),
        time);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void CheckClickingEnabled(bool enabled) {
    CHECK(element_);
    EXPECT_EQ(element_->IsClickingEnabled(), enabled);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void CheckConsoleMessageAfterDelay(
      base::TimeDelta time,
      unsigned int expected_count,
      std::optional<String> expected_text = std::nullopt) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&DeferredChecker::CheckConsoleMessage,
                      base::Unretained(this), expected_count,
                      std::move(expected_text)),
        time);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void CheckConsoleMessage(unsigned int expected_count,
                           std::optional<String> expected_text = std::nullopt) {
    CHECK(main_frame_);
    auto& console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            main_frame_->Client())
            ->ConsoleMessages();
    EXPECT_EQ(console_messages.size(), expected_count);

    if (expected_text.has_value()) {
      EXPECT_TRUE(console_messages.back().Contains(expected_text.value()));
    }
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

 private:
  Persistent<HTMLPermissionElement> element_ = nullptr;
  Persistent<WebLocalFrameImpl> main_frame_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(HTMLPemissionElementTest, InitializeInnerText) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::VIDEO_CAPTURE,
                                 MojoPermissionStatus::ASK},
                                {blink::mojom::PermissionName::AUDIO_CAPTURE,
                                 MojoPermissionStatus::ASK},
                                {blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::ASK}});
  const struct {
    const char* type;
    String expected_text;
    bool precise_location = false;
  } kTestData[] = {{"geolocation", kGeolocationString},
                   {"microphone", kMicrophoneString},
                   {"camera", kCameraString},
                   {"camera microphone", kCameraMicrophoneString},
                   {"geolocation", kPreciseGeolocationString, true},
                   {"geolocation", kGeolocationString, false}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    if (data.precise_location) {
      permission_element->setAttribute(html_names::kPreciselocationAttr,
                                       AtomicString(""));
    }
    permission_element->setAttribute(html_names::kStyleAttr,
                                     AtomicString("width: auto; height: auto"));
    GetDocument().body()->AppendChild(permission_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    DOMRect* rect = permission_element->GetBoundingClientRect();
    EXPECT_NE(0, rect->width());
    EXPECT_NE(0, rect->height());
  }
}

TEST_F(HTMLPemissionElementTest, TranslateInnerText) {
  const struct {
    const char* lang_attr_value;
    String expected_text_ask;
    String expected_text_allowed;
  } kTestData[] = {
      // no language means the default string
      {"", kGeolocationString, kGeolocationAllowedString},
      // "pt" selects Portuguese
      {"pT", kGeolocationStringPt, kGeolocationAllowedStringPt},
      // "pt-br" selects brazilian Portuguese
      {"pt-BR", kGeolocationStringBr, kGeolocationAllowedStringBr},
      // "pt" and a country that has no defined separate translation falls back
      // to Portuguese
      {"Pt-cA", kGeolocationStringPt, kGeolocationAllowedStringPt},
      // "pt" and something that is not a country falls back to Portuguese
      {"PT-gIbbeRish", kGeolocationStringPt, kGeolocationAllowedStringPt},
      // unrecognized locale selects the default string
      {"gibBeRish", kGeolocationString, kGeolocationAllowedString},
      // try tamil to test non-english-alphabet-based language
      {"ta", kGeolocationStringTa, kGeolocationAllowedStringTa}};

  auto* permission_element = CreatePermissionElement("geolocation");
  // Calling one more time waiting for the cache observer.
  permission_service()->WaitForPermissionObserverAdded();
  permission_service()->WaitForPermissionObserverAdded();
  for (const auto& data : kTestData) {
    permission_element->setAttribute(html_names::kLangAttr,
                                     AtomicString(data.lang_attr_value));
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    EXPECT_EQ(
        data.expected_text_ask,
        permission_element->permission_text_span_for_testing()->innerText());

    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    EXPECT_EQ(
        data.expected_text_allowed,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

// Regression test for crbug.com/341875650, check that a detached layout tree
// permission element doesn't crash the renderer process.
TEST_F(HTMLPemissionElementTest, AfterDetachLayoutTreeCrashTest) {
  auto* permission_element = CreatePermissionElement("camera");
  RegistrationWaiter(permission_element).Wait();
  permission_element->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();
  RegistrationWaiter(permission_element).Wait();
  // We end up here if the renderer process did not crash.
}

TEST_F(HTMLPemissionElementTest, SetTypeAfterInsertedInto) {
  const struct {
    const char* type;
    MojoPermissionStatus status;
    String expected_text;
    bool precise_location = false;
  } kTestData[] = {
      {"geolocation", MojoPermissionStatus::ASK, kGeolocationString},
      {"microphone", MojoPermissionStatus::ASK, kMicrophoneString},
      {"camera", MojoPermissionStatus::ASK, kCameraString},
      {"geolocation", MojoPermissionStatus::DENIED, kGeolocationString},
      {"microphone", MojoPermissionStatus::DENIED, kMicrophoneString},
      {"camera", MojoPermissionStatus::DENIED, kCameraString},
      {"geolocation", MojoPermissionStatus::GRANTED, kGeolocationAllowedString},
      {"microphone", MojoPermissionStatus::GRANTED, kMicrophoneAllowedString},
      {"camera", MojoPermissionStatus::GRANTED, kCameraAllowedString},
      {"geolocation", MojoPermissionStatus::ASK, kPreciseGeolocationString,
       true},
      {"geolocation", MojoPermissionStatus::DENIED, kPreciseGeolocationString,
       true},
      {"geolocation", MojoPermissionStatus::GRANTED,
       kPreciseGeolocationAllowedString, true},

      // Only affects geolocation.
      {"camera", MojoPermissionStatus::GRANTED, kCameraAllowedString, true},
      {"microphone", MojoPermissionStatus::ASK, kMicrophoneString, true},
  };
  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->GetPermissionService();
    GetDocument().body()->AppendChild(permission_element);
    permission_service()->set_initial_statuses({data.status});
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    if (data.precise_location) {
      permission_element->setAttribute(html_names::kPreciselocationAttr,
                                       AtomicString(""));
    }
    RegistrationWaiter(permission_element).Wait();
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPemissionElementTest, SetInnerTextAfterRegistrationSingleElement) {
  const struct {
    const char* type;
    MojoPermissionStatus status;
    String expected_text;
    bool precise_location = false;
  } kTestData[] = {
      {"geolocation", MojoPermissionStatus::ASK, kGeolocationString},
      {"microphone", MojoPermissionStatus::ASK, kMicrophoneString},
      {"camera", MojoPermissionStatus::ASK, kCameraString},
      {"geolocation", MojoPermissionStatus::DENIED, kGeolocationString},
      {"microphone", MojoPermissionStatus::DENIED, kMicrophoneString},
      {"camera", MojoPermissionStatus::DENIED, kCameraString},
      {"geolocation", MojoPermissionStatus::GRANTED, kGeolocationAllowedString},
      {"microphone", MojoPermissionStatus::GRANTED, kMicrophoneAllowedString},
      {"camera", MojoPermissionStatus::GRANTED, kCameraAllowedString},
      {"geolocation", MojoPermissionStatus::ASK, kPreciseGeolocationString,
       true},
      {"geolocation", MojoPermissionStatus::DENIED, kPreciseGeolocationString,
       true},
      {"geolocation", MojoPermissionStatus::GRANTED,
       kPreciseGeolocationAllowedString, true},

      // Only affects geolocation.
      {"camera", MojoPermissionStatus::GRANTED, kCameraAllowedString, true},
      {"microphone", MojoPermissionStatus::ASK, kMicrophoneString, true},
  };
  for (const auto& data : kTestData) {
    auto* permission_element =
        CreatePermissionElement(data.type, data.precise_location);
    permission_service()->set_initial_statuses({data.status});
    RegistrationWaiter(permission_element).Wait();
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
    auto* permission_element = CreatePermissionElement("camera microphone");
    permission_service()->set_initial_statuses(
        {data.camera_status, data.microphone_status});
    RegistrationWaiter(permission_element).Wait();
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
    bool precise_location = false;
  } kTestData[] = {
      {"geolocation", PermissionName::GEOLOCATION, MojoPermissionStatus::ASK,
       kGeolocationString},
      {"microphone", PermissionName::AUDIO_CAPTURE, MojoPermissionStatus::ASK,
       kMicrophoneString},
      {"camera", PermissionName::VIDEO_CAPTURE, MojoPermissionStatus::ASK,
       kCameraString},
      {"geolocation", PermissionName::GEOLOCATION, MojoPermissionStatus::DENIED,
       kGeolocationString},
      {"microphone", PermissionName::AUDIO_CAPTURE,
       MojoPermissionStatus::DENIED, kMicrophoneString},
      {"camera", PermissionName::VIDEO_CAPTURE, MojoPermissionStatus::DENIED,
       kCameraString},
      {"geolocation", PermissionName::GEOLOCATION,
       MojoPermissionStatus::GRANTED, kGeolocationAllowedString},
      {"microphone", PermissionName::AUDIO_CAPTURE,
       MojoPermissionStatus::GRANTED, kMicrophoneAllowedString},
      {"camera", PermissionName::VIDEO_CAPTURE, MojoPermissionStatus::GRANTED,
       kCameraAllowedString},
      {"geolocation", PermissionName::GEOLOCATION, MojoPermissionStatus::ASK,
       kPreciseGeolocationString, true},
      {"geolocation", PermissionName::GEOLOCATION, MojoPermissionStatus::DENIED,
       kPreciseGeolocationString, true},
      {"geolocation", PermissionName::GEOLOCATION,
       MojoPermissionStatus::GRANTED, kPreciseGeolocationAllowedString, true}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        CreatePermissionElement(data.type, data.precise_location);
    // Calling one more time waiting for the cache observer.
    permission_service()->WaitForPermissionObserverAdded();
    permission_service()->WaitForPermissionObserverAdded();
    permission_service()->NotifyPermissionStatusChange(data.name, data.status);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    GetDocument().body()->RemoveChild(permission_element);
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
    auto* permission_element = CreatePermissionElement("camera microphone");
    // Calling one more time waiting for the cache observer.
    permission_service()->WaitForPermissionObserverAdded();
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

TEST_F(HTMLPemissionElementTest, InitialAndUpdatedPermissionStatus) {
  for (const auto initial_status :
       {MojoPermissionStatus::ASK, MojoPermissionStatus::DENIED,
        MojoPermissionStatus::GRANTED}) {
    CachedPermissionStatus::From(GetDocument().domWindow())
        ->SetPermissionStatusMap(
            {{blink::mojom::PermissionName::GEOLOCATION, initial_status}});
    V8PermissionState::Enum expected_initial_status =
        PermissionStatusV8Enum(initial_status);
    auto* permission_element = CreatePermissionElement("geolocation");
    permission_service()->set_initial_statuses({initial_status});
    // Calling one more time waiting for the cache observer.
    permission_service()->WaitForPermissionObserverAdded();
    permission_service()->WaitForPermissionObserverAdded();
    EXPECT_EQ(expected_initial_status,
              permission_element->initialPermissionStatus());
    EXPECT_EQ(expected_initial_status, permission_element->permissionStatus());

    for (const auto updated_status :
         {MojoPermissionStatus::ASK, MojoPermissionStatus::DENIED,
          MojoPermissionStatus::GRANTED}) {
      V8PermissionState::Enum expected_updated_status =
          PermissionStatusV8Enum(updated_status);
      permission_service()->NotifyPermissionStatusChange(
          PermissionName::GEOLOCATION, updated_status);
      // After an updated, the initial permission status remains the same and
      // just the permission status changes.
      EXPECT_EQ(expected_initial_status,
                permission_element->initialPermissionStatus());
      EXPECT_EQ(expected_updated_status,
                permission_element->permissionStatus());
    }
    GetDocument().body()->RemoveChild(permission_element);
  }
}

TEST_F(HTMLPemissionElementTest, InitialAndUpdatedPermissionStatusGrouped) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::VIDEO_CAPTURE,
                                 MojoPermissionStatus::ASK},
                                {blink::mojom::PermissionName::AUDIO_CAPTURE,
                                 MojoPermissionStatus::ASK}});
  auto* permission_element = CreatePermissionElement("camera microphone");
  permission_service()->set_initial_statuses(
      {MojoPermissionStatus::ASK, MojoPermissionStatus::DENIED});

  // Before receiving any status, it's assumed it is "prompt" since we don't
  // have a better idea.
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->permissionStatus());

  // Two permissoin observers should be added since it's a grouped permission
  // element.
  permission_service()->WaitForPermissionObserverAdded();
  permission_service()->WaitForPermissionObserverAdded();

  // Calling one more time waiting for the cache observer.
  permission_service()->WaitForPermissionObserverAdded();
  permission_service()->WaitForPermissionObserverAdded();

  // The status is the most restrictive of the two permissions. The initial
  // status never changes. camera: ASK, mic: DENIED
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::DENIED),
            permission_element->permissionStatus());

  // camera:ASK, mic: ASK
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::AUDIO_CAPTURE, MojoPermissionStatus::ASK);
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->permissionStatus());

  // camera:DENIED, mic: ASK
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::VIDEO_CAPTURE, MojoPermissionStatus::DENIED);
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::DENIED),
            permission_element->permissionStatus());

  // camera:DENIED, mic: GRANTED
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::AUDIO_CAPTURE, MojoPermissionStatus::GRANTED);
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::DENIED),
            permission_element->permissionStatus());

  // camera:GRANTED, mic: GRANTED
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::VIDEO_CAPTURE, MojoPermissionStatus::GRANTED);
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::ASK),
            permission_element->initialPermissionStatus());
  EXPECT_EQ(PermissionStatusV8Enum(MojoPermissionStatus::GRANTED),
            permission_element->permissionStatus());
}

class HTMLPemissionElementClickingEnabledTest
    : public HTMLPemissionElementTest {
 public:
  HTMLPemissionElementClickingEnabledTest()
      : HTMLPemissionElementTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~HTMLPemissionElementClickingEnabledTest() override = default;
};

TEST_F(HTMLPemissionElementClickingEnabledTest, UnclickableBeforeRegistered) {
  const struct {
    const char* type;
    String expected_text;
  } kTestData[] = {{"geolocation", kGeolocationString},
                   {"microphone", kMicrophoneString},
                   {"camera", kCameraString},
                   {"camera microphone", kCameraMicrophoneString}};
  for (const auto& data : kTestData) {
    auto* permission_element = CreatePermissionElement(data.type);
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ true);
    // Check if the element is still unclickable even after the default timeout
    // of `kRecentlyAttachedToLayoutTree`.
    FastForwardBy(base::Milliseconds(600));
    EXPECT_FALSE(permission_element->IsClickingEnabled());
    std::move(permission_service()->TakePEPCRegisteredCallback()).Run();
    FastForwardUntilNoTasksRemain();
    EXPECT_TRUE(permission_element->IsClickingEnabled());
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ false);
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
        base::BindRepeating(&TestPermissionService::BindHandle,
                            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    SimTest::TearDown();
  }

  TestPermissionService* permission_service() { return &permission_service_; }

  HTMLPermissionElement* CreatePermissionElement(
      Document& document,
      const char* permission,
      std::optional<const char*> precise_location = std::nullopt) {
    HTMLPermissionElement* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(document);
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(permission));
    if (precise_location.has_value()) {
      permission_element->setAttribute(html_names::kPreciselocationAttr,
                                       AtomicString(precise_location.value()));
    }
    document.body()->AppendChild(permission_element);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    return permission_element;
  }

 private:
  TestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;
  ScopedPermissionElementForTest scoped_feature_{true};
};

TEST_F(HTMLPemissionElementSimTest, InitializeGrantedText) {
  SimRequest resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  resource.Complete(R"(
    <body>
    </body>
  )");
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::VIDEO_CAPTURE,
                                 MojoPermissionStatus::GRANTED},
                                {blink::mojom::PermissionName::AUDIO_CAPTURE,
                                 MojoPermissionStatus::GRANTED},
                                {blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});
  const struct {
    const char* type;
    String expected_text;
  } kTestData[] = {{"geolocation", kGeolocationAllowedString},
                   {"microphone", kMicrophoneAllowedString},
                   {"camera", kCameraAllowedString},
                   {"camera microphone", kCameraMicrophoneAllowedString}};

  for (const auto& data : kTestData) {
    auto* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString(data.type));
    permission_element->setAttribute(html_names::kStyleAttr,
                                     AtomicString("width: auto; height: auto"));
    GetDocument().body()->AppendChild(permission_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    DOMRect* rect = permission_element->GetBoundingClientRect();
    EXPECT_NE(0, rect->width());
    EXPECT_NE(0, rect->height());
  }
}

TEST_F(HTMLPemissionElementSimTest, BlockedByPermissionsPolicy) {
  SimRequest main_resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  SimRequest first_iframe_resource("https://example.test/foo1.html",
                                   "text/html");
  SimRequest last_iframe_resource("https://example.test/foo2.html",
                                  "text/html");
  main_resource.Complete(R"(
    <body>
      <iframe src='https://example.test/foo1.html'
        allow="camera 'none';microphone 'none';geolocation 'none'">
      </iframe>
      <iframe src='https://example.test/foo2.html'
        allow="camera *;microphone *;geolocation *">
      </iframe>
    </body>
  )");
  first_iframe_resource.Finish();
  last_iframe_resource.Finish();

  auto* first_child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* last_child_frame = To<WebLocalFrameImpl>(MainFrame().LastChild());
  for (const char* permission : {"camera", "microphone", "geolocation"}) {
    auto* permission_element = CreatePermissionElement(
        *last_child_frame->GetFrame()->GetDocument(), permission);
    RegistrationWaiter(permission_element).Wait();
    // PermissionsPolicy passed with no console log.
    auto& last_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            last_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(last_console_messages.size(), 0u);

    CreatePermissionElement(*first_child_frame->GetFrame()->GetDocument(),
                            permission);
    permission_service()->set_pepc_registered_callback(
        base::BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();
    // Should console log a error message due to PermissionsPolicy
    auto& first_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            first_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(first_console_messages.size(), 2u);
    EXPECT_TRUE(first_console_messages.front().Contains(
        "is not allowed in the current context due to PermissionsPolicy"));
    first_console_messages.clear();
    permission_service()->set_pepc_registered_callback(base::NullCallback());
  }
}

TEST_F(HTMLPemissionElementSimTest, EnableClickingAfterDelay) {
  auto* permission_element = CreatePermissionElement(GetDocument(), "camera");
  DeferredChecker checker(permission_element);
  permission_element->DisableClickingIndefinitely(
      HTMLPermissionElement::DisableReason::kInvalidStyle);
  checker.CheckClickingEnabled(/*enabled=*/false);

  // Calling |EnableClickingAfterDelay| for a reason that is currently disabling
  // clicking will result in clicking becoming enabled after the delay.
  permission_element->EnableClickingAfterDelay(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kDefaultTimeout);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);

  // Calling |EnableClickingAfterDelay| for a reason that is currently *not*
  // disabling clicking does not do anything.
  permission_element->EnableClickingAfterDelay(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kDefaultTimeout);
  checker.CheckClickingEnabled(/*enabled=*/true);
}

TEST_F(HTMLPemissionElementSimTest, BadContrastDisablesElement) {
  auto* permission_element = CreatePermissionElement(GetDocument(), "camera");
  DeferredChecker checker(permission_element);
  // Red on white is sufficient contrast.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: white;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":invalid-style")));

  // Red on purple is not sufficient contrast.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  EXPECT_TRUE(To<HTMLPermissionElement>(
                  GetDocument().QuerySelector(AtomicString("permission")))
                  ->matches(AtomicString(":invalid-style")));

  // Purple on yellow is sufficient contrast, the element will be re-enabled
  // after a delay.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: yellow; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":invalid-style")));

  // Purple on yellow is sufficient contrast, however the alpha is not at 100%
  // so the element should become disabled. rgba(255, 255, 0, 0.99) is "yellow"
  // at 99% alpha.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "color: rgba(255, 255, 0, 0.99); background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
}

TEST_F(HTMLPemissionElementSimTest, FontSizeCanDisableElement) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  auto* permission_element = CreatePermissionElement(GetDocument(), "camera");
  DeferredChecker checker(permission_element);

  // Normal font-size for baseline.
  permission_element->setAttribute(html_names::kStyleAttr,
                                   AtomicString("font-size: normal;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);

  struct {
    std::string fontSizeString;
    bool enabled;
  } kTests[] = {
      // px values.
      {"2px", false},
      {"100px", false},
      {"20px", true},
      // Keywords
      {"xlarge", true},
      // em based values
      {"1.5em", true},
      {"0.5em", false},
      {"6em", false},
      // Calculation values
      {"min(2px, 20px)", false},
      {"max(xsmall, large)", true},
  };

  std::string font_size_string;

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.fontSizeString);
    font_size_string = "font-size: " + test.fontSizeString + ";";
    permission_element->setAttribute(html_names::kStyleAttr,
                                     AtomicString(font_size_string.c_str()));
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    checker.CheckClickingEnabledAfterDelay(kDefaultTimeout, test.enabled);
    permission_element->EnableClicking(
        HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree);
    permission_element->EnableClicking(HTMLPermissionElement::DisableReason::
                                           kIntersectionRecentlyFullyVisible);
    permission_element->EnableClicking(
        HTMLPermissionElement::DisableReason::kInvalidStyle);

    EXPECT_TRUE(permission_element->IsClickingEnabled());
  }
}

class HTMLPemissionElementDispatchValidationEventTest
    : public HTMLPemissionElementSimTest {
 public:
  HTMLPemissionElementDispatchValidationEventTest() = default;

  ~HTMLPemissionElementDispatchValidationEventTest() override = default;

  HTMLPermissionElement* CreateElementAndWaitForRegistration() {
    auto& document = GetDocument();
    HTMLPermissionElement* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(document);
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString("camera"));
    permission_element->setAttribute(
        html_names::kOnvalidationstatuschangeAttr,
        AtomicString("console.log('event dispatched')"));
    document.body()->AppendChild(permission_element);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    DeferredChecker checker(permission_element, &MainFrame());
    checker.CheckConsoleMessage(/*expected_count*/ 1u, "event dispatched");
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(), "unsuccessful_registration");
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ true);
    checker.CheckConsoleMessageAfterDelay(base::Milliseconds(600),
                                          /*expected_count*/ 1u,
                                          "event dispatched");
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(), "unsuccessful_registration");
    std::move(permission_service()->TakePEPCRegisteredCallback()).Run();
    RegistrationWaiter(permission_element).Wait();
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ false);
    return permission_element;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test receiving event after registration
TEST_F(HTMLPemissionElementDispatchValidationEventTest, Registration) {
  auto* permission_element = CreateElementAndWaitForRegistration();
  DeferredChecker checker(permission_element, &MainFrame());
  checker.CheckConsoleMessage(
      /*expected_count*/ 2u, "event dispatched");
  EXPECT_TRUE(permission_element->isValid());
}

// Test receiving event after several times disabling (temporarily or
// indefinitely) + enabling a single reason and verify the `isValid` and
// `invalidReason` attrs.
TEST_F(HTMLPemissionElementDispatchValidationEventTest, DisableEnableClicking) {
  const struct {
    HTMLPermissionElement::DisableReason reason;
    String expected_invalid_reason;
  } kTestData[] = {
      {HTMLPermissionElement::DisableReason::kIntersectionRecentlyFullyVisible,
       String("intersection_visible")},
      {HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
       String("recently_attached")},
      {HTMLPermissionElement::DisableReason::kInvalidStyle,
       String("style_invalid")}};
  for (const auto& data : kTestData) {
    auto* permission_element = CreateElementAndWaitForRegistration();
    DeferredChecker checker(permission_element, &MainFrame());
    checker.CheckConsoleMessage(
        /*expected_count*/ 2u);
    EXPECT_TRUE(permission_element->isValid());
    permission_element->DisableClickingIndefinitely(data.reason);
    base::RunLoop().RunUntilIdle();
    checker.CheckConsoleMessage(
        /*expected_count*/ 3u, "event dispatched");
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    // Calling |DisableClickingTemporarily| for a reason that is currently
    // disabling clicking does not do anything.
    permission_element->DisableClickingTemporarily(data.reason,
                                                   base::Milliseconds(600));
    checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                          /*expected_count*/ 3u,
                                          "event dispatched");
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    // Calling |EnableClickingAfterDelay| for a reason that is currently
    // disabling clicking will result in a validation change event.
    permission_element->EnableClickingAfterDelay(data.reason, kSmallTimeout);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                          /*expected_count*/ 4u,
                                          "event dispatched");
    EXPECT_TRUE(permission_element->isValid());
    // Calling |EnableClickingAfterDelay| for a reason that is currently *not*
    // disabling clicking does not do anything.
    permission_element->EnableClickingAfterDelay(data.reason, kSmallTimeout);
    checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                          /*expected_count*/ 4u);

    permission_element->DisableClickingTemporarily(data.reason, kSmallTimeout);
    base::RunLoop().RunUntilIdle();
    checker.CheckConsoleMessage(
        /*expected_count*/ 5u, "event dispatched");
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                          /*expected_count*/ 6u,
                                          "event dispatched");
    EXPECT_TRUE(permission_element->isValid());

    GetDocument().body()->RemoveChild(permission_element);
    ConsoleMessages().clear();
  }
}

// Test restart the timer caused by `DisableClickingTemporarily` or
// `EnableClickingAfterDelay`. And verify that `invalidReason` changing could
// result in an event.
TEST_F(HTMLPemissionElementDispatchValidationEventTest,
       ChangeReasonRestartTimer) {
  auto* permission_element = CreateElementAndWaitForRegistration();
  DeferredChecker checker(permission_element, &MainFrame());
  checker.CheckConsoleMessage(
      /*expected_count*/ 2u, "event dispatched");
  EXPECT_TRUE(permission_element->isValid());
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
      kSmallTimeout);
  base::RunLoop().RunUntilIdle();
  checker.CheckConsoleMessage(
      /*expected_count*/ 3u, "event dispatched");
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "recently_attached");
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kDefaultTimeout);
  // Reason change to the "longest alive" reason, in this case is
  // `kInvalidStyle`
  base::RunLoop().RunUntilIdle();
  checker.CheckConsoleMessage(/*expected_count*/ 4u, "event dispatched");
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
      base::Milliseconds(100));
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  permission_element->EnableClickingAfterDelay(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kSmallTimeout);
  checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                        /*expected_count*/ 5u);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "recently_attached");
  checker.CheckConsoleMessageAfterDelay(kSmallTimeout,
                                        /*expected_count*/ 6u,
                                        "event dispatched");
  EXPECT_TRUE(permission_element->isValid());
}

// Test receiving event after disabling (temporarily or indefinitely) + enabling
// multiple reasons and verify the `isValid` and `invalidReason` attrs.
TEST_F(HTMLPemissionElementDispatchValidationEventTest,
       DisableEnableClickingDifferentReasons) {
  auto* permission_element = CreateElementAndWaitForRegistration();
  DeferredChecker checker(permission_element, &MainFrame());
  checker.CheckConsoleMessage(
      /*expected_count*/ 2u, "event dispatched");
  EXPECT_TRUE(permission_element->isValid());
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kIntersectionRecentlyFullyVisible,
      kDefaultTimeout);
  base::RunLoop().RunUntilIdle();
  checker.CheckConsoleMessage(
      /*expected_count*/ 3u, "event dispatched");
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "intersection_visible");

  // Disable indefinitely will stop the timer.
  permission_element->DisableClickingIndefinitely(
      HTMLPermissionElement::DisableReason::kInvalidStyle);
  base::RunLoop().RunUntilIdle();
  // `invalidReason` change from temporary `intersection` to indefinitely
  // `style`
  checker.CheckConsoleMessage(
      /*expected_count*/ 4u, "event dispatched");
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  checker.CheckConsoleMessageAfterDelay(kDefaultTimeout,
                                        /*expected_count*/ 4u);
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kIntersectionRecentlyFullyVisible,
      kDefaultTimeout);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");

  // Enable the indefinitely disabling reason, the timer will start with the
  // remaining temporary reason in the map.
  permission_element->EnableClicking(
      HTMLPermissionElement::DisableReason::kInvalidStyle);
  base::RunLoop().RunUntilIdle();
  // `invalidReason` change from `style` to temporary `intersection`
  checker.CheckConsoleMessage(
      /*expected_count*/ 5u, "event dispatched");
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "intersection_visible");
  checker.CheckConsoleMessageAfterDelay(kDefaultTimeout,
                                        /*expected_count*/ 6u,
                                        "event dispatched");
  EXPECT_TRUE(permission_element->isValid());
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
  SimRequest resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  resource.Complete(R"(
    <body>
    </body>
  )");

  for (const char* permission : {"camera", "microphone", "geolocation"}) {
    auto* permission_element = CreatePermissionElement(
        *MainFrame().GetFrame()->GetDocument(), permission);
    // We need this call to establish binding to the remote permission service,
    // otherwise the next testing binder will fail.
    permission_element->GetPermissionService();
    permission_service()->set_pepc_registered_callback(
        base::BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(HTMLPemissionElementSimTest, BlockedByMissingFrameAncestorsCSP) {
  SimRequest::Params params;
  params.response_http_headers = {
      {"content-security-policy",
       "frame-ancestors 'self' https://example.test"}};
  SimRequest main_resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  SimRequest first_iframe_resource("https://cross-example.test/foo1.html",
                                   "text/html");
  SimRequest last_iframe_resource("https://cross-example.test/foo2.html",
                                  "text/html", params);
  main_resource.Complete(R"(
    <body>
      <iframe src='https://cross-example.test/foo1.html'
        allow="camera *;microphone *;geolocation *">
      </iframe>
      <iframe src='https://cross-example.test/foo2.html'
        allow="camera *;microphone *;geolocation *">
      </iframe>
    </body>
  )");
  first_iframe_resource.Finish();
  last_iframe_resource.Finish();

  auto* first_child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* last_child_frame = To<WebLocalFrameImpl>(MainFrame().LastChild());
  for (const char* permission : {"camera", "microphone", "geolocation"}) {
    auto* permission_element = CreatePermissionElement(
        *last_child_frame->GetFrame()->GetDocument(), permission);
    RegistrationWaiter(permission_element).Wait();
    auto& last_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            last_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(last_console_messages.size(), 0u);

    CreatePermissionElement(*first_child_frame->GetFrame()->GetDocument(),
                            permission);
    permission_service()->set_pepc_registered_callback(
        base::BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();
    // Should console log a error message due to missing 'frame-ancestors' CSP
    auto& first_console_messages =
        static_cast<frame_test_helpers::TestWebFrameClient*>(
            first_child_frame->Client())
            ->ConsoleMessages();
    EXPECT_EQ(first_console_messages.size(), 2u);
    EXPECT_TRUE(first_console_messages.front().Contains(
        "is not allowed without the CSP 'frame-ancestors' directive present."));
    first_console_messages.clear();
    permission_service()->set_pepc_registered_callback(base::NullCallback());
  }
}

// Test that a permission element can be hidden (and shown again) by using the
// ":granted" pseudo-class selector.
TEST_F(HTMLPemissionElementSimTest, GrantedSelectorDisplayNone) {
  SimRequest main_resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  main_resource.Complete(R"(
    <body>
    <style>
      permission:granted { display: none; }
    </style>
    </body>
  )");

  auto* permission_element =
      CreatePermissionElement(GetDocument(), "geolocation");
  // Calling one more time waiting for the cache observer.
  permission_service()->WaitForPermissionObserverAdded();
  permission_service()->WaitForPermissionObserverAdded();
  EXPECT_TRUE(permission_element->GetComputedStyle());
  EXPECT_EQ(
      EDisplay::kInlineBlock,
      permission_element->GetComputedStyle()->GetDisplayStyle().Display());

  // The permission becomes granted, hiding the permission element because of
  // the style.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // An element with "display: none" does not have a computed style.
  EXPECT_FALSE(permission_element->GetComputedStyle());

  // The permission stops being granted, the permission element is no longer
  // hidden.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::DENIED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_TRUE(permission_element->GetComputedStyle());
  EXPECT_EQ(
      EDisplay::kInlineBlock,
      permission_element->GetComputedStyle()->GetDisplayStyle().Display());
}

// TODO(crbug.com/375231573): We should verify this test again. It's likely when
// moving PEPC between documents, the execution context binding to permission
// service will be changed.
TEST_F(HTMLPemissionElementSimTest, DISABLED_MovePEPCToAnotherDocument) {
  SimRequest main_resource("https://example.test/", "text/html");
  SimRequest iframe_resource("https://example.test/foo.html", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
      <iframe src='https://example.test/foo.html'
        allow="camera *">
      </iframe>
  </body>
  )HTML");
  iframe_resource.Finish();

  Compositor().BeginFrame();
  auto* permission_element =
      CreatePermissionElement(*MainFrame().GetFrame()->GetDocument(), "camera");
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto& new_document = *child_frame->GetFrame()->GetDocument();
  new_document.body()->AppendChild(permission_element);
  permission_service()->WaitForClientDisconnected();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

class HTMLPemissionElementIntersectionTest
    : public HTMLPemissionElementSimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLPemissionElementIntersectionTest() = default;

  void SetUp() override {
    HTMLPemissionElementSimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    HTMLPemissionElementSimTest::TearDown();
  }

  void WaitForIntersectionVisibilityChanged(
      HTMLPermissionElement* element,
      HTMLPermissionElement::IntersectionVisibility visibility) {
    // The intersection observer might only detect elements that enter/leave the
    // viewport after a cycle is complete.
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(element->IntersectionVisibilityForTesting(), visibility);
  }

  void TestContainerStyleAffectsVisibility(
      CSSPropertyID property_name,
      const String& property_value,
      HTMLPermissionElement::IntersectionVisibility expect_visibility) {
    SimRequest main_resource("https://example.test/", "text/html");
    LoadURL("https://example.test/");
    main_resource.Complete(R"HTML(
    <div id='container' style='position: fixed; left: 100px; top: 100px; width: 100px; height: 100px;'>
      <permission id='camera' type='camera'>
    </div>
    )HTML");

    Compositor().BeginFrame();
    auto* permission_element = To<HTMLPermissionElement>(
        GetDocument().QuerySelector(AtomicString("permission")));
    auto* div =
        To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));

    WaitForIntersectionVisibilityChanged(
        permission_element,
        HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
    DeferredChecker checker(permission_element);
    checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                           /*expected_enabled*/ true);

    div->SetInlineStyleProperty(property_name, property_value);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    WaitForIntersectionVisibilityChanged(permission_element, expect_visibility);
    checker.CheckClickingEnabled(/*expected_enabled*/ false);
  }
};

TEST_F(HTMLPemissionElementIntersectionTest, IntersectionChanged) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 100px;'></div>
    <permission id='camera' type='camera'>
    <div id='trailing' style='height: 700px;'></div>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, kViewportHeight), mojom::blink::ScrollType::kUser);
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOutOfViewportOrClipped);
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ false);
  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, -kViewportHeight), mojom::blink::ScrollType::kUser);

  // The element is fully visible now but unclickable for a short delay.
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  EXPECT_EQ(permission_element->IntersectionVisibilityForTesting(),
            HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  EXPECT_TRUE(permission_element->IsClickingEnabled());
}

TEST_F(HTMLPemissionElementIntersectionTest,
       IntersectionVisibleOverlapsRecentAttachedInterval) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 700px;'></div>
    <permission id='camera' type='camera'>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOutOfViewportOrClipped);
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
      base::Milliseconds(600));
  DeferredChecker checker(permission_element);

  checker.CheckClickingEnabledAfterDelay(base::Milliseconds(300),
                                         /*expected_enabled*/ false);
  // The `kIntersectionRecentlyFullyVisible` cooldown time which is overlapping
  // `kRecentlyAttachedToLayoutTree` will not extend the cooldown time, just
  // change the disable reason.
  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, kViewportHeight), mojom::blink::ScrollType::kUser);
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  permission_element->EnableClicking(
      HTMLPermissionElement::DisableReason::kIntersectionWithViewportChanged);
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  EXPECT_FALSE(permission_element->isValid());
  checker.CheckClickingEnabledAfterDelay(base::Milliseconds(300),
                                         /*expected_enabled*/ true);
  EXPECT_TRUE(permission_element->isValid());
}

TEST_F(HTMLPemissionElementIntersectionTest,
       IntersectionChangedDisableEnableDisable) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='cover' style='position: fixed; left: 0px; top: 100px; width: 100px; height: 100px;'></div>
    <permission id='camera' type='camera'>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  auto* div =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);

  // Placing the div over the element disables it.
  div->SetInlineStyleProperty(CSSPropertyID::kTop, "0px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);

  // Moving the div again will re-enable the element after a delay. Deliberately
  // don't make any calls that result in calling
  // PermissionElement::IsClickingEnabled.
  div->SetInlineStyleProperty(CSSPropertyID::kTop, "100px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Placing the div over the element disables it again.
  div->SetInlineStyleProperty(CSSPropertyID::kTop, "0px");
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ false);
  auto& console_messages =
      static_cast<frame_test_helpers::TestWebFrameClient*>(MainFrame().Client())
          ->ConsoleMessages();
  EXPECT_EQ(console_messages.size(), 2u);
  EXPECT_EQ(
      console_messages.front(),
      String::Format("The permission element 'camera' cannot be activated due "
                     "to intersection occluded or distorted."));
  EXPECT_EQ(console_messages.back(),
            String::Format("The permission element is occluded by node %s",
                           div->ToString().Utf8().c_str()));
}

TEST_F(HTMLPemissionElementIntersectionTest, ClickingDisablePseudoClass) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <div id='cover'
      style='position: fixed; left: 0px; top: 100px; width: 100px; height: 100px;'>
    </div>
    <permission id='camera' type='camera'>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  auto* div =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":occluded")));

  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: white;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":invalid-style")));

  // Red on purple is not sufficient contrast.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  EXPECT_TRUE(To<HTMLPermissionElement>(
                  GetDocument().QuerySelector(AtomicString("permission")))
                  ->matches(AtomicString(":invalid-style")));

  // Move the div to overlap the Permission Element
  div->SetInlineStyleProperty(CSSPropertyID::kTop, "0px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(To<HTMLPermissionElement>(
                  GetDocument().QuerySelector(AtomicString("permission")))
                  ->matches(AtomicString(":occluded")));
  div->SetInlineStyleProperty(CSSPropertyID::kTop, "100px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":occluded")));

  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: yellow; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);
  EXPECT_FALSE(To<HTMLPermissionElement>(
                   GetDocument().QuerySelector(AtomicString("permission")))
                   ->matches(AtomicString(":invalid-style")));
}

TEST_F(HTMLPemissionElementIntersectionTest, ContainerDivRotates) {
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kTransform, "rotate(0.1turn)",
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
}

TEST_F(HTMLPemissionElementIntersectionTest, ContainerDivOpacity) {
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kOpacity, "0.9",
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
}

TEST_F(HTMLPemissionElementIntersectionTest, ContainerDivClipPath) {
  // Set up a mask that covers a bit of the container.
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kClipPath, "circle(40%)",
      HTMLPermissionElement::IntersectionVisibility::kOutOfViewportOrClipped);
}

class HTMLPemissionElementLayoutChangeTest
    : public HTMLPemissionElementSimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLPemissionElementLayoutChangeTest() = default;

  void SetUp() override {
    HTMLPemissionElementSimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    HTMLPemissionElementSimTest::TearDown();
  }

  HTMLPermissionElement* CheckAndQueryPermissionElement(AtomicString element) {
    auto* permission_element =
        To<HTMLPermissionElement>(GetDocument().QuerySelector(element));
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(permission_element->IntersectionVisibilityForTesting(),
              HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
    DeferredChecker checker(permission_element);
    checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                           /*expected_enabled*/ true);
    return permission_element;
  }
};

TEST_F(HTMLPemissionElementLayoutChangeTest, InvalidatePEPCAfterMove) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style='position: relative; top: 1px; left: 1px;'
      id='camera'
      type='camera'>
  </body>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element =
      CheckAndQueryPermissionElement(AtomicString("permission"));
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("position: relative; top: 100px; left: 100px"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

TEST_F(HTMLPemissionElementLayoutChangeTest, InvalidatePEPCAfterResize) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style=' height: 3em; width: 40px;' id='camera' type='camera'>
  </body>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element =
      CheckAndQueryPermissionElement(AtomicString("permission"));
  permission_element->setAttribute(html_names::kStyleAttr,
                                   AtomicString(" height: 1em; width: 30px;"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

TEST_F(HTMLPemissionElementLayoutChangeTest, InvalidatePEPCAfterMoveContainer) {
  SimRequest main_resource("https://example.test/", "text/html");
  SimRequest iframe_resource("https://example.test/foo.html", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
      <iframe src='https://example.test/foo.html'
        allow="camera *">
      </iframe>
  </body>
  )HTML");
  iframe_resource.Finish();

  Compositor().BeginFrame();
  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* permission_element = CreatePermissionElement(
      *child_frame->GetFrame()->GetDocument(), "camera");
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().QuerySelector(AtomicString("iframe")));
  iframe->setAttribute(
      html_names::kStyleAttr,
      AtomicString("position: relative; top: 100px; left: 100px"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

TEST_F(HTMLPemissionElementLayoutChangeTest,
       InvalidatePEPCAfterTransformContainer) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='container'>
      <permission id='camera' type='camera'>
    </div>
    )HTML");
  Compositor().BeginFrame();
  auto* permission_element =
      CheckAndQueryPermissionElement(AtomicString("permission"));
  auto* div =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  div->SetInlineStyleProperty(CSSPropertyID::kTransform, "translateX(10px)");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

TEST_F(HTMLPemissionElementLayoutChangeTest,
       InvalidatePEPCLayoutInAnimationFrameCallback) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style=' height: 3em; width: 40px;' id='camera' type='camera'>
  </body>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element =
      CheckAndQueryPermissionElement(AtomicString("permission"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  // Run an animation frame callback which mutates the style of the element and
  // causes a synchronous style update. This should not result in an
  // "intersection changed" lifecycle state update, but still lock the element
  // temporarily.
  ClassicScript::CreateUnspecifiedScript(
      "window.requestAnimationFrame(function() {\n"
      "  var camera = document.getElementById('camera');\n"
      "  camera.style.width = '10px';\n"
      "  camera.getBoundingClientRect();\n"
      "  camera.style.width = '40px';\n"
      "\n"
      "});\n")
      ->RunScript(&Window());
  Compositor().BeginFrame();
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(permission_element->IsClickingEnabled());
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
}

}  // namespace blink
