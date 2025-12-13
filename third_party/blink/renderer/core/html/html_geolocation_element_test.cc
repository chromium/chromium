// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include <optional>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_generated_strings.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
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

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using MojoPermissionStatus = mojom::blink::PermissionStatus;

namespace {

constexpr char16_t kGeolocationStringPt[] = u"Usar localização";
constexpr char16_t kGeolocationStringBr[] = u"Usar local";
constexpr char16_t kGeolocationStringTa[] = u"இருப்பிடத்தைப் பயன்படுத்து";

constexpr char kGeolocationString[] = "Use location";
constexpr char kPreciseGeolocationString[] = "Use precise location";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    switch (resource_id) {
      case IDS_PERMISSION_REQUEST_GEOLOCATION:
        return kGeolocationString;
      case IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION:
        return kPreciseGeolocationString;
      case IDS_PERMISSION_REQUEST_GEOLOCATION_pt_PT:
        return WebString::FromUTF16(kGeolocationStringPt);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_pt_BR:
        return WebString::FromUTF16(kGeolocationStringBr);
      case IDS_PERMISSION_REQUEST_GEOLOCATION_ta:
        return WebString::FromUTF16(kGeolocationStringTa);
      default:
        break;
    }
    return TestingPlatformSupport::QueryLocalizedString(resource_id);
  }
};

}  // namespace

class HTMLGeolocationElementTestBase : public PageTestBase {
 protected:
  HTMLGeolocationElementTestBase() = default;

  explicit HTMLGeolocationElementTestBase(
      base::test::TaskEnvironment::TimeSource time_source)
      : PageTestBase(time_source) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kGeolocationElement);
    PageTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedGeolocationElementForTest scoped_feature_{true};
};

TEST_F(HTMLGeolocationElementTestBase, GetTypeAttribute) {
  auto* geolocation_element =
      MakeGarbageCollected<HTMLGeolocationElement>(GetDocument());
  EXPECT_EQ(AtomicString("geolocation"), geolocation_element->GetType());
  geolocation_element->setType(AtomicString("camera"));
  EXPECT_EQ(AtomicString("geolocation"), geolocation_element->GetType());
}

class HTMLGeolocationElementTest : public HTMLGeolocationElementTestBase {
 protected:
  HTMLGeolocationElementTest()
      : HTMLGeolocationElementTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    HTMLGeolocationElementTestBase::SetUp();
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        blink::BindRepeating(
            &PermissionElementTestPermissionService::BindHandle,
            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    HTMLGeolocationElementTestBase::TearDown();
  }

  PermissionElementTestPermissionService* permission_service() {
    return &permission_service_;
  }

  HTMLGeolocationElement* CreateGeolocationElement(
      bool precise_accuracy_mode = false) {
    HTMLGeolocationElement* geolocation_element =
        MakeGarbageCollected<HTMLGeolocationElement>(GetDocument());
    if (precise_accuracy_mode) {
      geolocation_element->setAttribute(html_names::kAccuracymodeAttr,
                                        AtomicString("precise"));
    }
    GetDocument().body()->AppendChild(geolocation_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return geolocation_element;
  }

  void CheckAppearance(HTMLGeolocationElement* element,
                       const String& expected_text,
                       bool is_in_progress) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    CheckInnerText(element, expected_text);
    EXPECT_EQ(is_in_progress,
              element->InProgressApearanceStartedTimeForTesting() !=
                  base::TimeTicks());
  }

  void CheckInnerText(HTMLGeolocationElement* element,
                      const String& expected_text) {
    base::RunLoop run_loop;
    // `UpdateText` is called via `PostTask`, so `innerText` is checked within a
    // `PostTask` to ensure it's updated.
    GetDocument()
        .GetTaskRunner(TaskType::kInternalDefault)
        ->PostTask(
            FROM_HERE,
            blink::BindOnce(
                [](HTMLGeolocationElement* element, const String& expected_text,
                   base::RepeatingClosure quit_closure) {
                  EXPECT_EQ(
                      expected_text,
                      element->permission_text_span_for_testing()->innerText());
                  quit_closure.Run();
                },
                WeakPersistent(element), expected_text,
                run_loop.QuitClosure()));
    run_loop.Run();
  }

 private:
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc_security_for_testing_{
      /*enabled=*/true};
  PermissionElementTestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support_;
};

TEST_F(HTMLGeolocationElementTest, GeolocationTranslateInnerText) {
  const struct {
    const char* lang_attr_value;
    String expected_text_ask;
  } kTestData[] = {
      // no language means the default string
      {"", kGeolocationString},
      // "pt" selects Portuguese
      {"pT", kGeolocationStringPt},
      // "pt-br" selects brazilian Portuguese
      {"pt-BR", kGeolocationStringBr},
      // "pt" and a country that has no defined separate translation falls back
      // to Portuguese
      {"Pt-cA", kGeolocationStringPt},
      // "pt" and something that is not a country falls back to Portuguese
      {"PT-gIbbeRish", kGeolocationStringPt},
      // unrecognized locale selects the default string
      {"gibBeRish", kGeolocationString},
      // try tamil to test non-english-alphabet-based language
      {"ta", kGeolocationStringTa}};

  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  for (const auto& data : kTestData) {
    geolocation_element->setAttribute(html_names::kLangAttr,
                                      AtomicString(data.lang_attr_value));
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
    CheckAppearance(geolocation_element, data.expected_text_ask,
                    /*is_in_progress*/ false);

    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
    // Simulate success response
    geolocation_element->CurrentPositionCallback(base::ok(nullptr));

    // Text should NOT change to the "allowed" string.
    CheckAppearance(geolocation_element, data.expected_text_ask,
                    /*is_in_progress*/ false);
  }
}

TEST_F(HTMLGeolocationElementTest, GeolocationSetInnerTextAfterRegistration) {
  const struct {
    MojoPermissionStatus status;
    String expected_text;
    bool precise_accuracy_mode = false;
  } kTestData[] = {
      {MojoPermissionStatus::ASK, kGeolocationString},
      {MojoPermissionStatus::DENIED, kGeolocationString},
      {MojoPermissionStatus::GRANTED, kGeolocationString},
      {MojoPermissionStatus::ASK, kPreciseGeolocationString, true},
      {MojoPermissionStatus::DENIED, kPreciseGeolocationString, true},
      {MojoPermissionStatus::GRANTED, kPreciseGeolocationString, true},
  };
  for (const auto& data : kTestData) {
    auto* geolocation_element =
        CreateGeolocationElement(data.precise_accuracy_mode);
    permission_service()->set_initial_statuses({data.status});
    WaitForPermissionElementRegistration(geolocation_element);
    CheckInnerText(geolocation_element, data.expected_text);
  }
}

TEST_F(HTMLGeolocationElementTest,
       GeolocationPreciseLocationAttributeDoesNotChangeText) {
  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  String initial_text =
      geolocation_element->permission_text_span_for_testing()->innerText();
  CheckInnerText(geolocation_element, initial_text);
  geolocation_element->setAttribute(html_names::kPreciselocationAttr,
                                    AtomicString(""));
  CheckInnerText(geolocation_element, initial_text);
}

TEST_F(HTMLGeolocationElementTest,
       GeolocationPreciseLocationAttributeCamelCaseDoesNotChangeText) {
  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  String initial_text =
      geolocation_element->permission_text_span_for_testing()->innerText();
  CheckInnerText(geolocation_element, initial_text);
  geolocation_element->setAttribute(AtomicString("pReCiSeLoCaTiOn"),
                                    AtomicString(""));
  CheckInnerText(geolocation_element, initial_text);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAccuracyMode) {
  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  geolocation_element->setAttribute(html_names::kAccuracymodeAttr,
                                    AtomicString("precise"));
  CheckInnerText(geolocation_element, kPreciseGeolocationString);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAccuracyModeCaseInsensitive) {
  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  geolocation_element->setAttribute(html_names::kAccuracymodeAttr,
                                    AtomicString("PrEcIsE"));
  CheckInnerText(geolocation_element, kPreciseGeolocationString);
}

TEST_F(HTMLGeolocationElementTest, GeolocationStatusChange) {
  const struct {
    MojoPermissionStatus status;
    String expected_text;
    bool precise_accuracy_mode = false;
  } kTestData[] = {
      {MojoPermissionStatus::ASK, kGeolocationString},
      {MojoPermissionStatus::DENIED, kGeolocationString},
      {MojoPermissionStatus::GRANTED, kGeolocationString},
      {MojoPermissionStatus::ASK, kPreciseGeolocationString, true},
      {MojoPermissionStatus::DENIED, kPreciseGeolocationString, true},
      {MojoPermissionStatus::GRANTED, kPreciseGeolocationString, true}};
  for (const auto& data : kTestData) {
    auto* geolocation_element =
        CreateGeolocationElement(data.precise_accuracy_mode);
    WaitForPermissionElementRegistration(geolocation_element);
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, data.status);
    CheckInnerText(geolocation_element, data.expected_text);
    GetDocument().body()->RemoveChild(geolocation_element);
  }
}

TEST_F(HTMLGeolocationElementTest, GeolocationGrantedClickBehavior) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  // Test with kWatchAttr
  auto* geolocation_element_watch = CreateGeolocationElement();
  geolocation_element_watch->setAttribute(html_names::kWatchAttr,
                                          AtomicString(""));
  auto* event_watch = Event::Create(event_type_names::kDOMActivate);
  geolocation_element_watch->DefaultEventHandler(*event_watch);
  CheckAppearance(geolocation_element_watch, kGeolocationString,
                  /*is_in_progress*/ true);

  // Test without kWatchAttr
  auto* geolocation_element_get_position = CreateGeolocationElement();
  auto* event_get_position = Event::Create(event_type_names::kDOMActivate);
  geolocation_element_get_position->DefaultEventHandler(*event_get_position);
  CheckAppearance(geolocation_element_get_position, kGeolocationString,
                  /*is_in_progress*/ true);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocate) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));

  // Should trigger GetCurrentPosition automatically.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);

  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocateWatch) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kWatchAttr, AtomicString(""));
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));

  // Should trigger WatchPosition automatically.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);

  // Let's simulate a position update.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocateTriggersOnce) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));

  // Should trigger GetCurrentPosition automatically.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);

  // Let it finish.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);

  // Trigger lifecycle update again. It should not trigger autolocate again.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);
}

TEST_F(HTMLGeolocationElementTest, GeolocationRequestInProgress) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  auto* event = Event::Create(event_type_names::kDOMActivate);

  geolocation_element->DefaultEventHandler(*event);
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);
  auto first_request_time =
      geolocation_element->InProgressApearanceStartedTimeForTesting();

  geolocation_element->DefaultEventHandler(*event);
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);
  auto second_request_time =
      geolocation_element->InProgressApearanceStartedTimeForTesting();

  EXPECT_EQ(first_request_time, second_request_time);
}

TEST_F(HTMLGeolocationElementTest,
       RequestLocationAfterClickAndPermissionChanged) {
  // This test simulates the following scenario:
  // 1. A geolocation element with `autolocate` is present.
  // 2. Permission is initially granted, so request location triggers and
  // succeeds.
  //    This sets an internal flag `is_autolocate_triggered_` to true.
  // 3. Permission is then revoked by the user (e.g. in page settings).
  // 4. The user clicks the element to grant permission again.
  // 5. After permission is granted, request location should trigger again.
  // 6. Another permission change event from Ask->Granted should not trigger
  //    request location.

  // Start with permission GRANTED.
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});
  // Request location  should trigger automatically due to autolocate.
  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);

  // Let it finish.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);

  // Revoke permission.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Simulate a click. This should trigger a permission prompt.
  auto* event = Event::Create(event_type_names::kDOMActivate);
  geolocation_element->DefaultEventHandler(*event);

  // Grant permission.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Request location should trigger again.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);
  // Let it finish.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);
}

TEST_F(HTMLGeolocationElementTest, PermissionStatusChangeAfterDecided) {
  auto* geolocation_element = CreateGeolocationElement();
  WaitForPermissionElementRegistration(geolocation_element);
  geolocation_element->OnEmbeddedPermissionsDecided(
      mojom::EmbeddedPermissionControlResult::kGranted);
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ false);

  // Simulate a system permission update.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Request location should trigger.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_in_progress*/ true);
}

class HTMLGeolocationElementSimTest : public SimTest {
 public:
  HTMLGeolocationElementSimTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    SimTest::SetUp();
    feature_list_.InitAndEnableFeature(features::kGeolocationElement);
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

  HTMLGeolocationElement* CreateGeolocationElement(Document& document) {
    HTMLGeolocationElement* geolocation_element =
        MakeGarbageCollected<HTMLGeolocationElement>(document);
    document.body()->AppendChild(geolocation_element);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return geolocation_element;
  }

 private:
  PermissionElementTestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support;
  base::test::ScopedFeatureList feature_list_;
  ScopedGeolocationElementForTest scoped_feature_{true};
};

TEST_F(HTMLGeolocationElementSimTest, GeolocationInitializeGrantedText) {
  SimRequest resource("https://example.test", "text/html");
  LoadURL("https://example.test");
  resource.Complete(R"(
    <body>
    </body>
  )");
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement(GetDocument());
  geolocation_element->setAttribute(html_names::kStyleAttr,
                                    AtomicString("width: auto; height: auto"));
  GetDocument().body()->AppendChild(geolocation_element);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return kGeolocationString ==
           geolocation_element->permission_text_span_for_testing()->innerText();
  }));
  DOMRect* rect = geolocation_element->GetBoundingClientRect();
  EXPECT_NE(0, rect->width());
  EXPECT_NE(0, rect->height());
}

TEST_F(HTMLGeolocationElementSimTest, InvalidDisplayStyleElement) {
  auto* geolocation_element = CreateGeolocationElement(GetDocument());
  geolocation_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("display: contents; position: absolute;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/false);

  geolocation_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("display: block; position: absolute;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/true);
}

TEST_F(HTMLGeolocationElementSimTest, BadContrastDisablesElement) {
  auto* geolocation_element = CreateGeolocationElement(GetDocument());
  // Red on white is sufficient contrast.
  geolocation_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: white;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/true);

  // Red on purple is not sufficient contrast.
  geolocation_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: red; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/false);

  // Purple on yellow is sufficient contrast, the element will be re-enabled
  // after a delay.
  geolocation_element->setAttribute(
      html_names::kStyleAttr,
      AtomicString("color: yellow; background-color: purple;"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/true);
}

class HTMLGeolocationElementIntersectionTest
    : public HTMLGeolocationElementSimTest {
 public:
  static constexpr int kViewportWidth = 800;
  static constexpr int kViewportHeight = 600;

 protected:
  HTMLGeolocationElementIntersectionTest() = default;

  void SetUp() override {
    HTMLGeolocationElementSimTest::SetUp();
    IntersectionObserver::SetThrottleDelayEnabledForTesting(false);
    WebView().MainFrameWidget()->Resize(
        gfx::Size(kViewportWidth, kViewportHeight));
  }

  void TearDown() override {
    IntersectionObserver::SetThrottleDelayEnabledForTesting(true);
    HTMLGeolocationElementSimTest::TearDown();
  }

  void WaitForIntersectionVisibilityChanged(
      HTMLGeolocationElement* element,
      HTMLGeolocationElement::IntersectionVisibility visibility) {
    // The intersection observer might only detect elements that enter/leave
    // the viewport after a cycle is complete.
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(element->IntersectionVisibilityForTesting(), visibility);
  }
};

TEST_F(HTMLGeolocationElementIntersectionTest, IntersectionChanged) {
  GetDocument().GetSettings()->SetDefaultFontSize(12);
  SimRequest main_resource("https://example.test/", "text/html");
  LoadURL("https://example.test/");
  main_resource.Complete(R"HTML(
    <div id='heading' style='height: 100px;'></div>
    <geolocation id='geo'></geolocation>
    <div id='trailing' style='height: 700px;'></div>
  )HTML");

  Compositor().BeginFrame();
  auto* geolocation_element = To<HTMLGeolocationElement>(
      GetDocument().QuerySelector(AtomicString("geolocation")));
  WaitForIntersectionVisibilityChanged(
      geolocation_element,
      HTMLGeolocationElement::IntersectionVisibility::kFullyVisible);
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/true);

  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, kViewportHeight), mojom::blink::ScrollType::kUser);
  WaitForIntersectionVisibilityChanged(
      geolocation_element,
      HTMLGeolocationElement::IntersectionVisibility::kOutOfViewportOrClipped);
  EXPECT_FALSE(geolocation_element->IsClickingEnabled());
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/false);

  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, -kViewportHeight), mojom::blink::ScrollType::kUser);

  // The element is fully visible now but unclickable for a short delay.
  WaitForIntersectionVisibilityChanged(
      geolocation_element,
      HTMLGeolocationElement::IntersectionVisibility::kFullyVisible);
  EXPECT_FALSE(geolocation_element->IsClickingEnabled());
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(geolocation_element->IsClickingEnabled(),
            /*expected_enabled=*/true);

  EXPECT_EQ(geolocation_element->IntersectionVisibilityForTesting(),
            HTMLGeolocationElement::IntersectionVisibility::kFullyVisible);
  EXPECT_TRUE(geolocation_element->IsClickingEnabled());
}

}  // namespace blink
