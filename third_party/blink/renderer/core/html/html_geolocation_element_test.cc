// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include <optional>

#include "base/functional/bind.h"
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
constexpr char kUsingLocationString[] = "Using location...";

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    switch (resource_id) {
      case IDS_PERMISSION_REQUEST_GEOLOCATION:
        return kGeolocationString;
      case IDS_PERMISSION_REQUEST_PRECISE_GEOLOCATION:
        return kPreciseGeolocationString;
      case IDS_PERMISSION_REQUEST_USING_LOCATION:
        return kUsingLocationString;
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
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient>
          pending_client) override {
    Vector<MojoPermissionStatus> statuses =
        initial_statuses_.empty()
            ? Vector<MojoPermissionStatus>(permissions.size(),
                                           MojoPermissionStatus::ASK)
            : initial_statuses_;
    client_ = mojo::Remote<mojom::blink::EmbeddedPermissionControlClient>(
        std::move(pending_client));
    client_->OnEmbeddedPermissionControlRegistered(/*allowed=*/true,
                                                   std::move(statuses));
  }

  void RequestPageEmbeddedPermission(
      Vector<PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptors,
      RequestPageEmbeddedPermissionCallback callback) override {
    std::move(callback).Run(
        mojom::blink::EmbeddedPermissionControlResult::kGranted);
  }
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
  void AddCombinedPermissionObserver(
      PermissionDescriptorPtr permission,
      MojoPermissionStatus last_known_status,
      mojo::PendingRemote<PermissionObserver> observer) override {
    observers_.emplace_back(permission->name, mojo::Remote<PermissionObserver>(
                                                  std::move(observer)));
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

  void set_initial_statuses(const Vector<MojoPermissionStatus>& statuses) {
    initial_statuses_ = statuses;
  }

 private:
  mojo::ReceiverSet<PermissionService> receivers_;
  Vector<std::pair<PermissionName, mojo::Remote<PermissionObserver>>>
      observers_;
  Vector<MojoPermissionStatus> initial_statuses_;
  mojo::Remote<mojom::blink::EmbeddedPermissionControlClient> client_;
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
        base::BindRepeating(&TestPermissionService::BindHandle,
                            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    HTMLGeolocationElementTestBase::TearDown();
  }

  TestPermissionService* permission_service() { return &permission_service_; }

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
                       bool is_spinning) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(expected_text,
              element->permission_text_span_for_testing()->innerText());
    EXPECT_EQ(is_spinning, element->SpinningIconTimerForTesting().IsActive());
  }

 private:
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc_security_for_testing_{
      /*enabled=*/true};
  TestPermissionService permission_service_;
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
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));
  for (const auto& data : kTestData) {
    geolocation_element->setAttribute(html_names::kLangAttr,
                                      AtomicString(data.lang_attr_value));
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::ASK);
    CheckAppearance(geolocation_element, data.expected_text_ask,
                    /*is_spinning*/ false);

    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
    // Simulate success response
    task_environment().FastForwardBy(base::Seconds(3));
    geolocation_element->CurrentPositionCallback(base::ok(nullptr));

    // Text should NOT change to the "allowed" string.
    CheckAppearance(geolocation_element, data.expected_text_ask,
                    /*is_spinning*/ false);
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
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return geolocation_element->is_registered_in_browser_process();
    }));
    EXPECT_EQ(
        data.expected_text,
        geolocation_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLGeolocationElementTest,
       GeolocationPreciseLocationAttributeDoesNotChangeText) {
  auto* geolocation_element = CreateGeolocationElement();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));
  String initial_text =
      geolocation_element->permission_text_span_for_testing()->innerText();
  geolocation_element->setAttribute(html_names::kPreciselocationAttr,
                                    AtomicString(""));
  EXPECT_EQ(
      initial_text,
      geolocation_element->permission_text_span_for_testing()->innerText());
}

TEST_F(HTMLGeolocationElementTest,
       GeolocationPreciseLocationAttributeCamelCaseDoesNotChangeText) {
  auto* geolocation_element = CreateGeolocationElement();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));
  String initial_text =
      geolocation_element->permission_text_span_for_testing()->innerText();
  geolocation_element->setAttribute(AtomicString("pReCiSeLoCaTiOn"),
                                    AtomicString(""));
  EXPECT_EQ(
      initial_text,
      geolocation_element->permission_text_span_for_testing()->innerText());
}

TEST_F(HTMLGeolocationElementTest, GeolocationAccuracyMode) {
  auto* geolocation_element = CreateGeolocationElement();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));
  geolocation_element->setAttribute(html_names::kAccuracymodeAttr,
                                    AtomicString("precise"));
  EXPECT_EQ(
      kPreciseGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
}

TEST_F(HTMLGeolocationElementTest, GeolocationAccuracyModeCaseInsensitive) {
  auto* geolocation_element = CreateGeolocationElement();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));
  geolocation_element->setAttribute(html_names::kAccuracymodeAttr,
                                    AtomicString("PrEcIsE"));
  EXPECT_EQ(
      kPreciseGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
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
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return geolocation_element->is_registered_in_browser_process();
    }));
    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, data.status);
    EXPECT_EQ(
        data.expected_text,
        geolocation_element->permission_text_span_for_testing()->innerText());
    GetDocument().body()->RemoveChild(geolocation_element);
  }
}

TEST_F(HTMLGeolocationElementTest, GeolocationUsingLocationAppearance) {
  auto* geolocation_element = CreateGeolocationElement();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));

  // 1. Test GetCurrentPosition
  geolocation_element->GetCurrentPosition();
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Text should remain "using" even if permission is granted.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Simulate success response
  task_environment().FastForwardBy(base::Seconds(3));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // 2. Test GetCurrentPosition with error response
  geolocation_element->GetCurrentPosition();
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Simulate error response
  task_environment().FastForwardBy(base::Seconds(3));
  geolocation_element->CurrentPositionCallback(base::unexpected(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // 3. Test that the spinning icon and "using" text are displayed for at
  // least 2 seconds, even if the response is received earlier.
  geolocation_element->GetCurrentPosition();
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Fast forward time by 1 second.
  task_environment().FastForwardBy(base::Seconds(1));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Simulate receiving a response after 1 second.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Fast forward time by another 1.1 seconds, making the total time > 2
  // seconds.
  task_environment().FastForwardBy(base::Seconds(1.1));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // 4. Test that the spinning icon and "using" text are displayed until a
  // response is received, even if it takes longer than 2 seconds.
  geolocation_element->GetCurrentPosition();
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Fast forward time by 2.1 seconds.
  task_environment().FastForwardBy(base::Seconds(2.1));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ false);

  // Simulate receiving a response after 2.1 seconds.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // Dispatch a click event under granted.
  auto* event = Event::Create(event_type_names::kDOMActivate);
  geolocation_element->DefaultEventHandler(*event);
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  task_environment().FastForwardBy(base::Seconds(3));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);
}

TEST_F(HTMLGeolocationElementTest, GeolocationWatchPositionAppearance) {
  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kWatchAttr, AtomicString(""));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return geolocation_element->is_registered_in_browser_process();
  }));

  // 1. Call WatchPosition and check initial spinning.
  geolocation_element->WatchPosition();
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // 2. After 1s, simulate a position update. Spinning should continue because
  // it's re-triggered.
  task_environment().FastForwardBy(base::Seconds(1));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // 3. After another 2.1s, spinning should stop.
  task_environment().FastForwardBy(base::Seconds(2.1));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // 4. Simulate another position update, it should start spinning again.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // 5. Remove watch attribute.
  geolocation_element->removeAttribute(html_names::kWatchAttr);
  // Let the current spinning finish.
  task_environment().FastForwardBy(base::Seconds(2.1));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // 6. Simulate another position update. It should NOT start spinning again.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);
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
  CheckAppearance(geolocation_element_watch, kUsingLocationString,
                  /*is_spinning*/ true);

  // Test without kWatchAttr
  auto* geolocation_element_get_position = CreateGeolocationElement();
  auto* event_get_position = Event::Create(event_type_names::kDOMActivate);
  geolocation_element_get_position->DefaultEventHandler(*event_get_position);
  CheckAppearance(geolocation_element_get_position, kUsingLocationString,
                  /*is_spinning*/ true);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocate) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));

  // Should trigger GetCurrentPosition automatically.
  // This will result in "Using location..." text and spinning icon.
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Fast forward time to let the spinning stop.
  task_environment().FastForwardBy(base::Seconds(2.1));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocateWatch) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));
  geolocation_element->setAttribute(html_names::kWatchAttr, AtomicString(""));

  // Should trigger WatchPosition automatically.
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // With watch, it should re-trigger spinning.
  // Let's simulate a position update.
  task_environment().FastForwardBy(base::Seconds(1));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);
}

TEST_F(HTMLGeolocationElementTest, GeolocationAutolocateTriggersOnce) {
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});

  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));

  // Should trigger GetCurrentPosition automatically.
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Let it finish.
  task_environment().FastForwardBy(base::Seconds(2.1));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

  // Trigger lifecycle update again. It should not trigger autolocate again.
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);
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

  // Start with permission GRANTED.
  CachedPermissionStatus::From(GetDocument().domWindow())
      ->SetPermissionStatusMap({{blink::mojom::PermissionName::GEOLOCATION,
                                 MojoPermissionStatus::GRANTED}});
  // Request location  should trigger automatically due to autolocate.
  auto* geolocation_element = CreateGeolocationElement();
  geolocation_element->setAttribute(html_names::kAutolocateAttr,
                                    AtomicString(""));
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);

  // Let it finish.
  task_environment().FastForwardBy(base::Seconds(2.1));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  CheckAppearance(geolocation_element, kGeolocationString,
                  /*is_spinning*/ false);

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
  CheckAppearance(geolocation_element, kUsingLocationString,
                  /*is_spinning*/ true);
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
        base::BindRepeating(&TestPermissionService::BindHandle,
                            base::Unretained(&permission_service_)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    SimTest::TearDown();
  }

  TestPermissionService* permission_service() { return &permission_service_; }

  HTMLGeolocationElement* CreateGeolocationElement(Document& document) {
    HTMLGeolocationElement* geolocation_element =
        MakeGarbageCollected<HTMLGeolocationElement>(document);
    document.body()->AppendChild(geolocation_element);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return geolocation_element;
  }

 private:
  TestPermissionService permission_service_;
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
  EXPECT_EQ(
      kGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
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
