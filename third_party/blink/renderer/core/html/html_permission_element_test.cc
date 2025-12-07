// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element.h"

#include <optional>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
#include "third_party/blink/renderer/core/html/html_geolocation_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
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
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
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

constexpr char kValidationStatusChangeEvent[] =
    "onvalidationstatuschange event";

constexpr base::TimeDelta kLongerThanDefaultTimeout = base::Milliseconds(600);
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

protocol::Audits::PermissionElementIssueDetails* GetPermissionElementIssue(
    Document& document,
    protocol::Audits::PermissionElementIssueType issue_type,
    base::RepeatingCallback<
        bool(protocol::Audits::PermissionElementIssueDetails&)> matcher =
        base::NullCallback()) {
  auto& storage = document.GetPage()->GetInspectorIssueStorage();
  for (size_t i = 0; i < storage.size(); ++i) {
    auto* issue = storage.at(i);
    if (issue->getCode() ==
            protocol::Audits::InspectorIssueCodeEnum::PermissionElementIssue &&
        issue->getDetails()->hasPermissionElementIssueDetails()) {
      const auto& details =
          issue->getDetails()->getPermissionElementIssueDetails();
      if (details->getIssueType() == issue_type) {
        if (!matcher || matcher.Run(*details)) {
          return details.get();
        }
      }
    }
  }
  return nullptr;
}

size_t CountPermissionElementIssues(Document& document) {
  auto& storage = document.GetPage()->GetInspectorIssueStorage();
  size_t count = 0;
  for (size_t i = 0; i < storage.size(); ++i) {
    auto* issue = const_cast<protocol::Audits::InspectorIssue*>(storage.at(i));
    if (issue->getCode() ==
        protocol::Audits::InspectorIssueCodeEnum::PermissionElementIssue) {
      count++;
    }
  }

  return count;
}

}  // namespace

class HTMLPermissionElementTestBase : public PageTestBase {
 protected:
  HTMLPermissionElementTestBase() = default;

  HTMLPermissionElementTestBase(
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

TEST_F(HTMLPermissionElementTestBase, SetTypeAttribute) {
  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("camera"));
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("geolocation"));

  EXPECT_EQ(AtomicString("camera"), permission_element->GetType());
}

TEST_F(HTMLPermissionElementTestBase, SetPreciseLocationAttribute) {
  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(GetDocument());

  EXPECT_FALSE(permission_element->is_precise_location_);

  permission_element->setAttribute(html_names::kPreciselocationAttr,
                                   AtomicString(""));
  EXPECT_TRUE(permission_element->is_precise_location_);

  permission_element->removeAttribute(html_names::kPreciselocationAttr);
  EXPECT_FALSE(permission_element->is_precise_location_);
}

TEST_F(HTMLPermissionElementTestBase, ParsePermissionDescriptorsFromType) {
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
    std::ranges::transform(data.expected_permissions,
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

class HTMLPermissionElementTest : public HTMLPermissionElementTestBase {
 protected:
  HTMLPermissionElementTest() = default;

  HTMLPermissionElementTest(base::test::TaskEnvironment::TimeSource time_source)
      : HTMLPermissionElementTestBase(time_source) {}

  void SetUp() override {
    HTMLPermissionElementTestBase::SetUp();
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        blink::BindRepeating(
            &PermissionElementTestPermissionService::BindHandle,
            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    HTMLPermissionElementTestBase::TearDown();
  }

  PermissionElementTestPermissionService* permission_service() {
    return &permission_service_;
  }

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
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return permission_element;
  }

 private:
  PermissionElementTestPermissionService permission_service_;
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
    test::RunPendingTasks();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&DeferredChecker::CheckClickingEnabled, Unretained(this),
                 expected_enabled),
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

  void CheckNoNewMessagesAfterDelay(base::TimeDelta time) {
    size_t current_size = ConsoleMessages().size();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&DeferredChecker::CheckConsoleMessagesSize, Unretained(this),
                 current_size),
        time);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void CheckConsoleMessagesSize(size_t expected_size) {
    EXPECT_EQ(ConsoleMessages().size(), expected_size);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void CheckConsoleMessageAtIndex(unsigned int message_index,
                                  const String& expected_text) {
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return ConsoleMessages().size() > message_index; }));

    EXPECT_TRUE(ConsoleMessages()[message_index].Contains(expected_text));
  }

 private:
  Vector<String>& ConsoleMessages() {
    return static_cast<frame_test_helpers::TestWebFrameClient*>(
               main_frame_->Client())
        ->ConsoleMessages();
  }

  Persistent<HTMLPermissionElement> element_ = nullptr;
  Persistent<WebLocalFrameImpl> main_frame_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(HTMLPermissionElementTest, InitializeInnerText) {
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
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    DOMRect* rect = permission_element->GetBoundingClientRect();
    EXPECT_NE(0, rect->width());
    EXPECT_NE(0, rect->height());
  }
}

TEST_F(HTMLPermissionElementTest, TranslateInnerText) {
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
  WaitForPermissionElementRegistration(permission_element);
  for (const auto& data : kTestData) {
    permission_element->setAttribute(html_names::kLangAttr,
                                     AtomicString(data.lang_attr_value));
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(
        data.expected_text_ask,
        permission_element->permission_text_span_for_testing()->innerText());

    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(
        data.expected_text_allowed,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

// Regression test for crbug.com/341875650, check that a detached layout tree
// permission element doesn't crash the renderer process.
TEST_F(HTMLPermissionElementTest, AfterDetachLayoutTreeCrashTest) {
  auto* permission_element = CreatePermissionElement("camera");
  WaitForPermissionElementRegistration(permission_element);
  permission_element->SetForceReattachLayoutTree();
  UpdateAllLifecyclePhasesForTest();
  WaitForPermissionElementRegistration(permission_element);
  // We end up here if the renderer process did not crash.
}

TEST_F(HTMLPermissionElementTest, SetTypeAfterInsertedInto) {
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
    UpdateAllLifecyclePhasesForTest();
    WaitForPermissionElementRegistration(permission_element);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPermissionElementTest, SetInnerTextAfterRegistrationSingleElement) {
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
    WaitForPermissionElementRegistration(permission_element);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPermissionElementTest,
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
    WaitForPermissionElementRegistration(permission_element);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPermissionElementTest, StatusChangeSinglePermissionElement) {
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
    WaitForPermissionElementRegistration(permission_element);
    permission_service()->NotifyPermissionStatusChange(data.name, data.status);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
    GetDocument().body()->RemoveChild(permission_element);
  }
}

TEST_F(HTMLPermissionElementTest,
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
    WaitForPermissionElementRegistration(permission_element);
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::VIDEO_CAPTURE, data.camera_status);
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::AUDIO_CAPTURE, data.microphone_status);
    EXPECT_EQ(
        data.expected_text,
        permission_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLPermissionElementTest, InitialAndUpdatedPermissionStatus) {
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
    WaitForPermissionElementRegistration(permission_element);
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

TEST_F(HTMLPermissionElementTest, InitialAndUpdatedPermissionStatusGrouped) {
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

  WaitForPermissionElementRegistration(permission_element);

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

class HTMLPermissionElementClickingEnabledTest
    : public HTMLPermissionElementTest {
 public:
  HTMLPermissionElementClickingEnabledTest()
      : HTMLPermissionElementTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~HTMLPermissionElementClickingEnabledTest() override = default;
};

TEST_F(HTMLPermissionElementClickingEnabledTest, UnclickableBeforeRegistered) {
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

class HTMLPermissionElementSimTest : public SimTest {
 public:
  HTMLPermissionElementSimTest() = default;

  ~HTMLPermissionElementSimTest() override = default;

  void SetUp() override {
    SimTest::SetUp();
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        blink::BindRepeating(
            &PermissionElementTestPermissionService::BindHandle,
            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    SimTest::TearDown();
  }

  PermissionElementTestPermissionService* permission_service() {
    return &permission_service_;
  }

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
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return permission_element;
  }

 private:
  PermissionElementTestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;
  ScopedPermissionElementForTest scoped_feature_{true};
};

TEST_F(HTMLPermissionElementSimTest, GeolocationTypeDeprecationWarning) {
  SimRequest resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  resource.Complete(R"(
    <body>
    </body>
  )");

  // Check <permission type="geolocation">
  auto* permission_element =
      MakeGarbageCollected<HTMLPermissionElement>(GetDocument());
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("geolocation"));
  GetDocument().body()->AppendChild(permission_element);

  EXPECT_TRUE(GetPermissionElementIssue(
      GetDocument(),
      protocol::Audits::PermissionElementIssueTypeEnum::GeolocationDeprecated));
  EXPECT_EQ(CountPermissionElementIssues(GetDocument()), 1u);
  GetDocument().GetPage()->GetInspectorIssueStorage().Clear();

  // Check <geolocation>
  auto* geolocation_element =
      MakeGarbageCollected<HTMLGeolocationElement>(GetDocument());
  GetDocument().body()->AppendChild(geolocation_element);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(CountPermissionElementIssues(GetDocument()), 0u);
}

TEST_F(HTMLPermissionElementSimTest, InitializeGrantedText) {
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

TEST_F(HTMLPermissionElementSimTest, BlockedByPermissionsPolicy) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
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
  struct {
    const char* permission;
    const char* permissionName;
  } kTests[] = {
      {"camera", "video_capture"},
      {"microphone", "audio_capture"},
      {"geolocation", "geolocation"},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.permission);
    auto* last_doc = last_child_frame->GetFrame()->GetDocument();
    auto* first_doc = first_child_frame->GetFrame()->GetDocument();

    // Test the frame in which the the policy allows the element
    EXPECT_FALSE(GetPermissionElementIssue(
        GetDocument(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            PermissionsPolicyBlocked,
        base::BindLambdaForTesting(
            [&](protocol::Audits::PermissionElementIssueDetails& details) {
              return details.getPermissionName("") == test.permissionName;
            })));
    auto* permission_element =
        CreatePermissionElement(*last_doc, test.permission);
    WaitForPermissionElementRegistration(permission_element);
    EXPECT_FALSE(GetPermissionElementIssue(
        GetDocument(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            PermissionsPolicyBlocked,
        base::BindLambdaForTesting(
            [&](protocol::Audits::PermissionElementIssueDetails& details) {
              return details.getPermissionName("") == test.permissionName;
            })));

    // Test the frame in which the policy denies the element
    first_doc->GetPage()->GetInspectorIssueStorage().Clear();
    EXPECT_EQ(CountPermissionElementIssues(*first_doc), 0u);
    CreatePermissionElement(*first_doc, test.permission);
    permission_service()->set_pepc_registered_callback(
        BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(GetPermissionElementIssue(
        GetDocument(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            PermissionsPolicyBlocked,
        base::BindLambdaForTesting(
            [&](protocol::Audits::PermissionElementIssueDetails& details) {
              return details.getPermissionName("") == test.permissionName;
            })));

    permission_service()->set_pepc_registered_callback(base::NullCallback());
  }
}

TEST_F(HTMLPermissionElementSimTest, EnableClickingAfterDelay) {
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

TEST_F(HTMLPermissionElementSimTest, InvalidDisplayStyleElement) {
  auto* permission_element = CreatePermissionElement(GetDocument(), "camera");
  DeferredChecker checker(permission_element);
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("display: contents; position: absolute;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/false);

  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("display: block; position: absolute;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);
}

TEST_F(HTMLPermissionElementSimTest, BadContrastDisablesElement) {
  auto* permission_element = CreatePermissionElement(GetDocument(), "camera");
  DeferredChecker checker(permission_element);
  // Red on white is sufficient contrast.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: white;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);

  // Red on purple is not sufficient contrast.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);

  // Purple on yellow is sufficient contrast, the element will be re-enabled
  // after a delay.
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: yellow; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  checker.CheckClickingEnabled(/*enabled=*/false);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/true);

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

TEST_F(HTMLPermissionElementSimTest, FontSizeCanDisableElement) {
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
    permission_element->EnableClicking(
        HTMLPermissionElement::DisableReason::
            kIntersectionVisibilityOccludedOrDistorted);
    permission_element->EnableClicking(
        HTMLPermissionElement::DisableReason::
            kIntersectionVisibilityOutOfViewPortOrClipped);
    permission_element->EnableClicking(
        HTMLPermissionElement::DisableReason::kInvalidStyle);

    EXPECT_TRUE(permission_element->IsClickingEnabled());
  }
}

TEST_F(HTMLPermissionElementSimTest, RegisterAfterBeingVisible) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style='display:block; visibility:hidden'
      id='camera'></permission>
  </body>
  )HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      permission_element->is_registered_in_browser_process_for_testing());
  permission_element->setAttribute(html_names::kTypeAttr,
                                   AtomicString("camera"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      permission_element->is_registered_in_browser_process_for_testing());
  permission_element->setAttribute(html_names::kStyleAttr,
                                   AtomicString("visibility:visible;"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  WaitForPermissionElementRegistration(permission_element);
  permission_element->setAttribute(html_names::kStyleAttr,
                                   AtomicString("display: none"));
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      permission_element->is_registered_in_browser_process_for_testing());
}

class HTMLPermissionElementDispatchValidationEventTest
    : public HTMLPermissionElementSimTest {
 public:
  HTMLPermissionElementDispatchValidationEventTest() = default;

  ~HTMLPermissionElementDispatchValidationEventTest() override = default;

  HTMLPermissionElement*
  CreateElementAndWaitForPermissionElementRegistration() {
    auto& document = GetDocument();
    HTMLPermissionElement* permission_element =
        MakeGarbageCollected<HTMLPermissionElement>(document);
    permission_element->setAttribute(html_names::kTypeAttr,
                                     AtomicString("camera"));
    permission_element->setAttribute(
        html_names::kOnvalidationstatuschangeAttr,
        AtomicString("console.log('onvalidationstatuschange event')"));
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ true);
    document.body()->AppendChild(permission_element);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    DeferredChecker checker(permission_element, &MainFrame());
    checker.CheckConsoleMessageAtIndex(0u, kValidationStatusChangeEvent);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(), "unsuccessful_registration");
    checker.CheckNoNewMessagesAfterDelay(kLongerThanDefaultTimeout);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(), "unsuccessful_registration");
    std::move(permission_service()->TakePEPCRegisteredCallback()).Run();
    WaitForPermissionElementRegistration(permission_element);
    permission_service()->set_should_defer_registered_callback(
        /*should_defer*/ false);
    checker.CheckConsoleMessageAtIndex(1u, kValidationStatusChangeEvent);
    ConsoleMessages().clear();
    return permission_element;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test receiving event after registration
TEST_F(HTMLPermissionElementDispatchValidationEventTest, Registration) {
  auto* permission_element =
      CreateElementAndWaitForPermissionElementRegistration();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return permission_element->isValid(); }));
}

// Test receiving event after several times disabling (temporarily or
// indefinitely) + enabling a single reason and verify the `isValid` and
// `invalidReason` attrs.
TEST_F(HTMLPermissionElementDispatchValidationEventTest,
       DisableEnableClicking) {
  const struct {
    HTMLPermissionElement::DisableReason reason;
    String expected_invalid_reason;
  } kTestData[] = {
      {HTMLPermissionElement::DisableReason::
           kIntersectionVisibilityOccludedOrDistorted,
       String("intersection_occluded_or_distorted")},
      {HTMLPermissionElement::DisableReason::
           kIntersectionVisibilityOutOfViewPortOrClipped,
       String("intersection_out_of_viewport_or_clipped")},
      {HTMLPermissionElement::DisableReason::kIntersectionWithViewportChanged,
       String("intersection_changed")},
      {HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
       String("recently_attached")},
      {HTMLPermissionElement::DisableReason::kInvalidStyle,
       String("style_invalid")}};
  for (const auto& data : kTestData) {
    auto* permission_element =
        CreateElementAndWaitForPermissionElementRegistration();
    DeferredChecker checker(permission_element, &MainFrame());
    EXPECT_TRUE(permission_element->isValid());
    permission_element->DisableClickingIndefinitely(data.reason);
    checker.CheckConsoleMessageAtIndex(0u, kValidationStatusChangeEvent);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    // Calling |DisableClickingTemporarily| for a reason that is currently
    // disabling clicking does not do anything.
    permission_element->DisableClickingTemporarily(data.reason,
                                                   base::Milliseconds(600));
    checker.CheckNoNewMessagesAfterDelay(kSmallTimeout);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    // Calling |EnableClickingAfterDelay| for a reason that is currently
    // disabling clicking will result in a validation change event.
    permission_element->EnableClickingAfterDelay(data.reason, kSmallTimeout);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    checker.CheckConsoleMessageAtIndex(1u, kValidationStatusChangeEvent);
    EXPECT_TRUE(permission_element->isValid());
    // Calling |EnableClickingAfterDelay| for a reason that is currently *not*
    // disabling clicking does not do anything.
    permission_element->EnableClickingAfterDelay(data.reason, kSmallTimeout);
    checker.CheckNoNewMessagesAfterDelay(kSmallTimeout);

    permission_element->DisableClickingTemporarily(data.reason, kSmallTimeout);
    checker.CheckConsoleMessageAtIndex(2u, kValidationStatusChangeEvent);
    EXPECT_FALSE(permission_element->isValid());
    EXPECT_EQ(permission_element->invalidReason(),
              data.expected_invalid_reason);
    checker.CheckConsoleMessageAtIndex(3u, kValidationStatusChangeEvent);
    EXPECT_TRUE(permission_element->isValid());

    GetDocument().body()->RemoveChild(permission_element);
    ConsoleMessages().clear();
  }
}

// Test restart the timer caused by `DisableClickingTemporarily` or
// `EnableClickingAfterDelay`. And verify that `invalidReason` changing could
// result in an event.
TEST_F(HTMLPermissionElementDispatchValidationEventTest,
       ChangeReasonRestartTimer) {
  auto* permission_element =
      CreateElementAndWaitForPermissionElementRegistration();
  DeferredChecker checker(permission_element, &MainFrame());
  EXPECT_TRUE(permission_element->isValid());
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
      kSmallTimeout);
  checker.CheckConsoleMessageAtIndex(0u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "recently_attached");
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kDefaultTimeout);
  // Reason change to the "longest alive" reason, in this case is
  // `kInvalidStyle`
  checker.CheckConsoleMessageAtIndex(1u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::kRecentlyAttachedToLayoutTree,
      base::Milliseconds(100));
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  permission_element->EnableClickingAfterDelay(
      HTMLPermissionElement::DisableReason::kInvalidStyle, kSmallTimeout);
  checker.CheckConsoleMessageAtIndex(2u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "recently_attached");
  checker.CheckConsoleMessageAtIndex(3u, kValidationStatusChangeEvent);
  EXPECT_TRUE(permission_element->isValid());
}

// Test receiving event after disabling (temporarily or indefinitely) + enabling
// multiple reasons and verify the `isValid` and `invalidReason` attrs.
TEST_F(HTMLPermissionElementDispatchValidationEventTest,
       DisableEnableClickingDifferentReasons) {
  auto* permission_element =
      CreateElementAndWaitForPermissionElementRegistration();
  DeferredChecker checker(permission_element, &MainFrame());
  EXPECT_TRUE(permission_element->isValid());
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::
          kIntersectionVisibilityOutOfViewPortOrClipped,
      kDefaultTimeout);
  checker.CheckConsoleMessageAtIndex(0u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(),
            "intersection_out_of_viewport_or_clipped");

  // Disable indefinitely will stop the timer.
  permission_element->DisableClickingIndefinitely(
      HTMLPermissionElement::DisableReason::kInvalidStyle);
  // `invalidReason` change from temporary `intersection` to indefinitely
  // `style`
  checker.CheckConsoleMessageAtIndex(1u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");
  checker.CheckNoNewMessagesAfterDelay(kDefaultTimeout);
  permission_element->DisableClickingTemporarily(
      HTMLPermissionElement::DisableReason::
          kIntersectionVisibilityOutOfViewPortOrClipped,
      kDefaultTimeout);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(), "style_invalid");

  // Enable the indefinitely disabling reason, the timer will start with the
  // remaining temporary reason in the map.
  permission_element->EnableClicking(
      HTMLPermissionElement::DisableReason::kInvalidStyle);
  // `invalidReason` change from `style` to temporary `intersection`
  checker.CheckConsoleMessageAtIndex(2u, kValidationStatusChangeEvent);
  EXPECT_FALSE(permission_element->isValid());
  EXPECT_EQ(permission_element->invalidReason(),
            "intersection_out_of_viewport_or_clipped");
  checker.CheckConsoleMessageAtIndex(3u, kValidationStatusChangeEvent);
  EXPECT_TRUE(permission_element->isValid());
}

class HTMLPermissionElementFencedFrameTest
    : public HTMLPermissionElementSimTest {
 public:
  HTMLPermissionElementFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  ~HTMLPermissionElementFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HTMLPermissionElementFencedFrameTest, NotAllowedInFencedFrame) {
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
        BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(HTMLPermissionElementSimTest, BlockedByMissingFrameAncestorsCSP) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
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
  struct {
    const char* permission;
    const char* type;
  } kTests[] = {
      {"camera", "video_capture"},
      {"microphone", "audio_capture"},
      {"geolocation", "geolocation"},
  };
  for (const auto& test : kTests) {
    auto* permission_element = CreatePermissionElement(
        *last_child_frame->GetFrame()->GetDocument(), test.permission);
    WaitForPermissionElementRegistration(permission_element);

    EXPECT_FALSE(GetPermissionElementIssue(
        GetDocument(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            CspFrameAncestorsMissing,
        base::BindLambdaForTesting(
            [&](protocol::Audits::PermissionElementIssueDetails& details) {
              return details.getType("") == test.permission;
            })));

    CreatePermissionElement(*first_child_frame->GetFrame()->GetDocument(),
                            test.permission);
    permission_service()->set_pepc_registered_callback(
        BindOnce(&NotReachedForPEPCRegistered));
    base::RunLoop().RunUntilIdle();

    // Should raise an issue with an error message due to missing
    // 'frame-ancestors' CSP
    EXPECT_TRUE(GetPermissionElementIssue(
        GetDocument(),
        protocol::Audits::PermissionElementIssueTypeEnum::
            CspFrameAncestorsMissing,
        base::BindLambdaForTesting(
            [&](protocol::Audits::PermissionElementIssueDetails& details) {
              return details.getType("") == test.permission;
            })));
    permission_service()->set_pepc_registered_callback(base::NullCallback());
  }
}

// Test that a permission element can be hidden (and shown again) by using the
// ":granted" pseudo-class selector.
TEST_F(HTMLPermissionElementSimTest, GrantedSelectorDisplayNone) {
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
  WaitForPermissionElementRegistration(permission_element);
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
TEST_F(HTMLPermissionElementSimTest, DISABLED_MovePEPCToAnotherDocument) {
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

class HTMLPermissionElementIntersectionTest
    : public HTMLPermissionElementSimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLPermissionElementIntersectionTest() = default;

  void SetUp() override {
    HTMLPermissionElementSimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    HTMLPermissionElementSimTest::TearDown();
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
    GetDocument().GetSettings()->SetDefaultFontSize(12);
    SimRequest main_resource("https://example.test/", "text/html");
    LoadURL("https://example.test/");
    main_resource.Complete(R"HTML(
    <div id='container' style='position: fixed; left: 100px; top: 100px; width: 100px; height: 100px;'>
      <permission id='camera' type='camera'></permission>
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

TEST_F(HTMLPermissionElementIntersectionTest, IntersectionChanged) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 100px;'></div>
    <permission id='camera' type='camera'></permission>
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

TEST_F(HTMLPermissionElementIntersectionTest,
       IntersectionVisibleOverlapsRecentAttachedInterval) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 700px;'></div>
    <permission id='camera' type='camera'></permission>
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
  // The recently visible cooldown time which is overlapping
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

TEST_F(HTMLPermissionElementIntersectionTest,
       IntersectionChangedDisableEnableDisable) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='cover' style='position: fixed; left: 0px; top: 100px; width: 100px; height: 100px;'></div>
    <permission id='camera' type='camera'></permission>
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
                                         /*expected_enabled=*/false);
  auto* issue = GetPermissionElementIssue(
      GetDocument(),
      protocol::Audits::PermissionElementIssueTypeEnum::ActivationDisabled);
  EXPECT_TRUE(issue);
  EXPECT_EQ(issue->getDisableReason(), "intersection occluded or distorted");
  EXPECT_TRUE(issue->getOccluderNodeInfo().value().Contains(div->ToString()));
}

TEST_F(HTMLPermissionElementIntersectionTest, IntersectionOccluderLogging) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
<div id='parent' style='width: 250px; height: 250px;'>
  <permission
      style='position: relative; border:0; top: 0px; left: 0px; width: 100px; height: 36px;'
      id='camera'
      type='camera'>
  </permission>
  <div style='position: relative; left: 0px; top: -36px; width: 2px; height: 2px;'>
</div>
)HTML");

  Compositor().BeginFrame();
  auto* permission_element = To<HTMLPermissionElement>(
      GetDocument().QuerySelector(AtomicString("permission")));
  auto* parent_div =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  auto* div =
      To<HTMLDivElement>(parent_div->QuerySelector(AtomicString("div")));
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kFullyVisible);
  DeferredChecker checker(permission_element);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled*/ true);
  permission_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "position: relative; border:0; top: 0px; left: 0px; width: 100px; "
          "height: 36px; color: red; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  div->SetInlineStyleProperty(CSSPropertyID::kTop, "-33px");
  div->SetInlineStyleProperty(CSSPropertyID::kLeft, "3px");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  WaitForIntersectionVisibilityChanged(
      permission_element,
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
  checker.CheckClickingEnabledAfterDelay(kDefaultTimeout,
                                         /*expected_enabled=*/false);
  EXPECT_TRUE(GetPermissionElementIssue(
      GetDocument(),
      protocol::Audits::PermissionElementIssueTypeEnum::LowContrast));
  EXPECT_TRUE(GetPermissionElementIssue(
      GetDocument(),
      protocol::Audits::PermissionElementIssueTypeEnum::ActivationDisabled,
      base::BindLambdaForTesting(
          [&](protocol::Audits::PermissionElementIssueDetails& details) {
            return details.getDisableReason("") == "invalid style";
          })));
  auto* issue = GetPermissionElementIssue(
      GetDocument(),
      protocol::Audits::PermissionElementIssueTypeEnum::ActivationDisabled,
      base::BindLambdaForTesting(
          [&](protocol::Audits::PermissionElementIssueDetails& details) {
            return details.getDisableReason("") ==
                   "intersection occluded or distorted";
          }));
  EXPECT_TRUE(issue);
  EXPECT_TRUE(issue->hasOccluderNodeInfo());
  EXPECT_TRUE(issue->getOccluderNodeInfo().value().Contains(div->ToString()));
  EXPECT_TRUE(issue->hasOccluderParentNodeInfo());
  EXPECT_TRUE(issue->getOccluderParentNodeInfo().value().Contains(
      parent_div->ToString()));
  EXPECT_EQ(CountPermissionElementIssues(GetDocument()), 3u);
}

#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ContainerDivRotates DISABLED_ContainerDivRotates
#else
#define MAYBE_ContainerDivRotates ContainerDivRotates
#endif
TEST_F(HTMLPermissionElementIntersectionTest, MAYBE_ContainerDivRotates) {
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kTransform, "rotate(0.1turn)",
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
}

#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ContainerDivOpacity DISABLED_ContainerDivOpacity
#else
#define MAYBE_ContainerDivOpacity ContainerDivOpacity
#endif
TEST_F(HTMLPermissionElementIntersectionTest, MAYBE_ContainerDivOpacity) {
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kOpacity, "0.9",
      HTMLPermissionElement::IntersectionVisibility::kOccludedOrDistorted);
}

#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_ContainerDivClipPath DISABLED_ContainerDivClipPath
#else
#define MAYBE_ContainerDivClipPath ContainerDivClipPath
#endif
TEST_F(HTMLPermissionElementIntersectionTest, MAYBE_ContainerDivClipPath) {
  // Set up a mask that covers a bit of the container.
  TestContainerStyleAffectsVisibility(
      CSSPropertyID::kClipPath, "circle(40%)",
      HTMLPermissionElement::IntersectionVisibility::kOutOfViewportOrClipped);
}

class HTMLPermissionElementLayoutChangeTest
    : public HTMLPermissionElementSimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLPermissionElementLayoutChangeTest() = default;

  void SetUp() override {
    HTMLPermissionElementSimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    HTMLPermissionElementSimTest::TearDown();
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

TEST_F(HTMLPermissionElementLayoutChangeTest, InvalidatePEPCAfterMove) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style='position: relative; top: 1px; left: 1px;'
      id='camera'
      type='camera'></permission>
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

TEST_F(HTMLPermissionElementLayoutChangeTest, InvalidatePEPCAfterResize) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style=' height: 3em; width: 40px;' id='camera' type='camera'></permission>
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

TEST_F(HTMLPermissionElementLayoutChangeTest,
       InvalidatePEPCAfterMoveContainer) {
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

TEST_F(HTMLPermissionElementLayoutChangeTest,
       InvalidatePEPCAfterTransformContainer) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='container'>
      <permission id='camera' type='camera'></permission>
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

TEST_F(HTMLPermissionElementLayoutChangeTest,
       InvalidatePEPCLayoutInAnimationFrameCallback) {
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
  <body>
    <permission
      style=' height: 3em; width: 40px;' id='camera' type='camera'></permission>
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
