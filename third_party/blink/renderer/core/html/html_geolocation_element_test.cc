// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_geolocation_element.h"

#include <optional>

#include "base/run_loop.h"
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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
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
      bool precise_location = false) {
    HTMLGeolocationElement* geolocation_element =
        MakeGarbageCollected<HTMLGeolocationElement>(GetDocument());
    if (precise_location) {
      geolocation_element->setAttribute(html_names::kPreciselocationAttr,
                                        AtomicString(""));
    }
    GetDocument().body()->AppendChild(geolocation_element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    return geolocation_element;
  }

 private:
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
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(
        data.expected_text_ask,
        geolocation_element->permission_text_span_for_testing()->innerText());

    permission_service()->NotifyPermissionStatusChange(
        PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
    // Simulate success response
    task_environment().FastForwardBy(base::Seconds(3));
    geolocation_element->CurrentPositionCallback(base::ok(nullptr));

    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    // Text should NOT change to the "allowed" string.
    EXPECT_EQ(
        data.expected_text_ask,
        geolocation_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLGeolocationElementTest, GeolocationSetInnerTextAfterRegistration) {
  const struct {
    MojoPermissionStatus status;
    String expected_text;
    bool precise_location = false;
  } kTestData[] = {
      {MojoPermissionStatus::ASK, kGeolocationString},
      {MojoPermissionStatus::DENIED, kGeolocationString},
      {MojoPermissionStatus::GRANTED, kGeolocationString},
      {MojoPermissionStatus::ASK, kPreciseGeolocationString, true},
      {MojoPermissionStatus::DENIED, kPreciseGeolocationString, true},
      {MojoPermissionStatus::GRANTED, kPreciseGeolocationString, true},
  };
  for (const auto& data : kTestData) {
    auto* geolocation_element = CreateGeolocationElement(data.precise_location);
    permission_service()->set_initial_statuses({data.status});
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return geolocation_element->is_registered_in_browser_process();
    }));
    EXPECT_EQ(
        data.expected_text,
        geolocation_element->permission_text_span_for_testing()->innerText());
  }
}

TEST_F(HTMLGeolocationElementTest, GeolocationStatusChange) {
  const struct {
    MojoPermissionStatus status;
    String expected_text;
    bool precise_location = false;
  } kTestData[] = {
      {MojoPermissionStatus::ASK, kGeolocationString},
      {MojoPermissionStatus::DENIED, kGeolocationString},
      {MojoPermissionStatus::GRANTED, kUsingLocationString},
      {MojoPermissionStatus::ASK, kPreciseGeolocationString, true},
      {MojoPermissionStatus::DENIED, kPreciseGeolocationString, true},
      {MojoPermissionStatus::GRANTED, kUsingLocationString, true}};
  for (const auto& data : kTestData) {
    auto* geolocation_element = CreateGeolocationElement(data.precise_location);
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
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Text should remain "using" even if permission is granted.
  permission_service()->NotifyPermissionStatusChange(
      PermissionName::GEOLOCATION, MojoPermissionStatus::GRANTED);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());

  // Simulate success response
  task_environment().FastForwardBy(base::Seconds(3));
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_FALSE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // 2. Test GetCurrentPosition with error response
  geolocation_element->GetCurrentPosition();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Simulate error response
  task_environment().FastForwardBy(base::Seconds(3));
  geolocation_element->CurrentPositionCallback(base::unexpected(nullptr));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_FALSE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // 3. Test that the spinning icon and "using" text are displayed for at
  // least 2 seconds, even if the response is received earlier.
  geolocation_element->GetCurrentPosition();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Fast forward time by 1 second.
  task_environment().FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Simulate receiving a response after 1 second.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Fast forward time by another 1.1 seconds, making the total time > 2
  // seconds.
  task_environment().FastForwardBy(base::Seconds(1.1));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_FALSE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // 4. Test that the spinning icon and "using" text are displayed until a
  // response is received, even if it takes longer than 2 seconds.
  geolocation_element->GetCurrentPosition();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_TRUE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Fast forward time by 2.1 seconds.
  task_environment().FastForwardBy(base::Seconds(2.1));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kUsingLocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_FALSE(geolocation_element->SpinningIconTimerForTesting().IsActive());

  // Simulate receiving a response after 2.1 seconds.
  geolocation_element->CurrentPositionCallback(base::ok(nullptr));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      kGeolocationString,
      geolocation_element->permission_text_span_for_testing()->innerText());
  EXPECT_FALSE(geolocation_element->SpinningIconTimerForTesting().IsActive());
}

class HTMLGeolocationElementSimTest : public SimTest {
 public:
  HTMLGeolocationElementSimTest() = default;

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

  HTMLGeolocationElement* CreateGeolocationElement(
      Document& document,
      std::optional<const char*> precise_location = std::nullopt) {
    HTMLGeolocationElement* geolocation_element =
        MakeGarbageCollected<HTMLGeolocationElement>(document);
    if (precise_location.has_value()) {
      geolocation_element->setAttribute(html_names::kPreciselocationAttr,
                                        AtomicString(precise_location.value()));
    }
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

}  // namespace blink
