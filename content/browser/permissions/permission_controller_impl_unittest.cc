// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include <cstdlib>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

using blink::PermissionType;

namespace content {

namespace {
using ::testing::Unused;
using OverrideStatus = PermissionControllerImpl::OverrideStatus;
using RequestsCallback =
    base::OnceCallback<void(const std::vector<PermissionStatus>&)>;

constexpr char kTestUrl[] = "https://google.com";

class MockManagerWithRequests : public MockPermissionManager {
 public:
  MockManagerWithRequests() {}

  MockManagerWithRequests(const MockManagerWithRequests&) = delete;
  MockManagerWithRequests& operator=(const MockManagerWithRequests&) = delete;

  ~MockManagerWithRequests() override {}
  MOCK_METHOD(
      void,
      RequestPermissionsFromCurrentDocument,
      (RenderFrameHost * render_frame_host,
       const PermissionRequestDescription& request_description,
       const base::OnceCallback<void(const std::vector<PermissionStatus>&)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      RequestPermissions,
      (RenderFrameHost * render_frame_host,
       const PermissionRequestDescription& request_description,
       const base::OnceCallback<void(const std::vector<PermissionStatus>&)>
           callback),
      (override));
  MOCK_METHOD(bool,
              IsPermissionOverridable,
              (PermissionType, const std::optional<url::Origin>&),
              (override));
};

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager() = default;
  ~TestPermissionManager() override = default;

  PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override {
    RenderFrameHost* top_frame = render_frame_host->GetParentOrOuterDocument();
    GURL url;

    if (top_frame) {
      url = top_frame->GetLastCommittedOrigin().GetURL();
    } else {
      url = render_frame_host->GetLastCommittedOrigin().GetURL();
    }

    if (override_status_.contains(url)) {
      return override_status_[url];
    }

    return PermissionStatus::ASK;
  }

  void SetPermissionStatus(GURL url, PermissionStatus status) {
    override_status_[url] = status;
  }

 private:
  std::map<GURL, PermissionStatus> override_status_;
};

// Results are defined based on assumption that same types are queried for
// each test case.
const struct {
  std::map<PermissionType, PermissionStatus> overrides;

  std::vector<PermissionType> delegated_permissions;
  std::vector<PermissionStatus> delegated_statuses;

  std::vector<PermissionStatus> expected_results;
  bool expect_death;
} kTestPermissionRequestCases[] = {
    // No overrides present - all delegated.
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {PermissionStatus::DENIED, PermissionStatus::GRANTED,
      PermissionStatus::GRANTED},
     {PermissionStatus::DENIED, PermissionStatus::GRANTED,
      PermissionStatus::GRANTED},
     /*expect_death=*/false},

    // No delegates needed - all overridden.
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC, PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, PermissionStatus::ASK}},
     {},
     {},
     {PermissionStatus::GRANTED, PermissionStatus::GRANTED,
      PermissionStatus::ASK},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::BACKGROUND_SYNC, PermissionStatus::DENIED}},
     {PermissionType::GEOLOCATION, PermissionType::MIDI_SYSEX},
     {PermissionStatus::GRANTED, PermissionStatus::ASK},
     {PermissionStatus::GRANTED, PermissionStatus::DENIED,
      PermissionStatus::ASK},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC, PermissionStatus::DENIED}},
     {PermissionType::MIDI_SYSEX},
     {PermissionStatus::ASK},
     {PermissionStatus::GRANTED, PermissionStatus::DENIED,
      PermissionStatus::ASK},
     /*expect_death=*/false},

    // Too many delegates (causes death).
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, PermissionStatus::ASK}},
     {PermissionType::BACKGROUND_SYNC},
     {PermissionStatus::DENIED, PermissionStatus::GRANTED},
     // Results don't matter because will die.
     {},
     /*expect_death=*/true},

    // Too few delegates (causes death).
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {PermissionStatus::GRANTED, PermissionStatus::GRANTED},
     // Results don't matter because will die.
     {},
     /*expect_death=*/true}};

}  // namespace

class PermissionControllerImplTest : public ::testing::Test {
 public:
  PermissionControllerImplTest() {
    browser_context_.SetPermissionControllerDelegate(
        std::make_unique<::testing::NiceMock<MockManagerWithRequests>>());
    permission_controller_ =
        std::make_unique<PermissionControllerImpl>(&browser_context_);
  }

  PermissionControllerImplTest(const PermissionControllerImplTest&) = delete;
  PermissionControllerImplTest& operator=(const PermissionControllerImplTest&) =
      delete;

  ~PermissionControllerImplTest() override {
    browser_context_.SetPermissionControllerDelegate(nullptr);
  }

  void SetUp() override {
    ON_CALL(*mock_manager(), IsPermissionOverridable)
        .WillByDefault(testing::Return(true));
  }

  PermissionControllerImpl* permission_controller() {
    return permission_controller_.get();
  }

  void PermissionControllerRequestPermissionsFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
    permission_controller()->RequestPermissionsFromCurrentDocument(
        render_frame_host, std::move(request_description), std::move(callback));
  }

  void PermissionControllerRequestPermissions(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
    permission_controller()->RequestPermissions(
        render_frame_host, std::move(request_description), std::move(callback));
  }

  PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) {
    return permission_controller()->GetPermissionStatusForWorker(
        permission, render_process_host, worker_origin);
  }

  PermissionStatus GetPermissionStatusForCurrentDocument(
      PermissionType permission,
      RenderFrameHost* render_frame_host) {
    return permission_controller()->GetPermissionStatusForCurrentDocument(
        permission, render_frame_host);
  }

  BrowserContext* browser_context() { return &browser_context_; }

  MockManagerWithRequests* mock_manager() {
    return static_cast<MockManagerWithRequests*>(
        browser_context_.GetPermissionControllerDelegate());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  std::unique_ptr<PermissionControllerImpl> permission_controller_;
};

TEST_F(PermissionControllerImplTest,
       RequestPermissionsFromCurrentDocumentDelegatesIffMissingOverrides) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));
  RenderViewHostTestEnabler enabler;

  const std::vector<PermissionType> kTypesToQuery = {
      PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX};

  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(
          WebContents::CreateParams(browser_context())));

  WebContentsTester* web_contents_tester =
      WebContentsTester::For(web_contents.get());
  web_contents_tester->NavigateAndCommit(GURL(kTestUrl));

  RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  for (const auto& test_case : kTestPermissionRequestCases) {
    // Need to reset overrides for each case to ensure delegation is as
    // expected.
    permission_controller()->ResetOverridesForDevTools();
    for (const auto& permission_status_pair : test_case.overrides) {
      permission_controller()->SetOverrideForDevTools(
          kTestOrigin, permission_status_pair.first,
          permission_status_pair.second);
    }

    // Expect request permission from current document calls if override are
    // missing.
    if (!test_case.delegated_permissions.empty()) {
      auto forward_callbacks = testing::WithArg<2>(
          [&test_case](
              base::OnceCallback<void(const std::vector<PermissionStatus>&)>
                  callback) {
            std::move(callback).Run(test_case.delegated_statuses);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(), RequestPermissionsFromCurrentDocument(
                                     rfh,
                                     PermissionRequestDescription(
                                         test_case.delegated_permissions,
                                         /*user_gesture*/ true, GURL(kTestUrl)),
                                     testing::_))
            .WillByDefault(testing::Invoke(forward_callbacks));
      } else {
        EXPECT_CALL(*mock_manager(),
                    RequestPermissionsFromCurrentDocument(
                        rfh,
                        PermissionRequestDescription(
                            test_case.delegated_permissions,
                            /*user_gesture*/ true, GURL(kTestUrl)),
                        testing::_))
            .WillOnce(testing::Invoke(forward_callbacks));
      }
    } else {
      // There should be no call to delegate if all overrides are defined.
      EXPECT_CALL(*mock_manager(), RequestPermissionsFromCurrentDocument)
          .Times(0);
    }

    if (test_case.expect_death) {
      GTEST_FLAG_SET(death_test_style, "threadsafe");
      base::MockCallback<RequestsCallback> callback;
      EXPECT_DEATH_IF_SUPPORTED(
          PermissionControllerRequestPermissionsFromCurrentDocument(
              rfh,
              PermissionRequestDescription(kTypesToQuery,
                                           /*user_gesture*/ true),
              callback.Get()),
          "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissionsFromCurrentDocument(
          rfh,
          PermissionRequestDescription(kTypesToQuery, /*user_gesture*/ true),
          callback.Get());
    }
  }
}

TEST_F(PermissionControllerImplTest,
       RequestPermissionsDelegatesIffMissingOverrides) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));
  RenderViewHostTestEnabler enabler;

  const std::vector<PermissionType> kTypesToQuery = {
      PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX};

  std::unique_ptr<WebContents> web_contents(
      WebContentsTester::CreateTestWebContents(
          WebContents::CreateParams(browser_context())));

  WebContentsTester* web_contents_tester =
      WebContentsTester::For(web_contents.get());
  url::Origin testing_origin = url::Origin::Create(GURL(kTestUrl));
  web_contents_tester->NavigateAndCommit(testing_origin.GetURL());

  RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  for (const auto& test_case : kTestPermissionRequestCases) {
    // Need to reset overrides for each case to ensure delegation is as
    // expected.
    permission_controller()->ResetOverridesForDevTools();
    for (const auto& permission_status_pair : test_case.overrides) {
      permission_controller()->SetOverrideForDevTools(
          kTestOrigin, permission_status_pair.first,
          permission_status_pair.second);
    }

    // Expect request permission call if override are missing.
    if (!test_case.delegated_permissions.empty()) {
      auto forward_callbacks = testing::WithArg<2>(
          [&test_case](
              base::OnceCallback<void(const std::vector<PermissionStatus>&)>
                  callback) {
            std::move(callback).Run(test_case.delegated_statuses);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissions(rfh,
                                   PermissionRequestDescription(
                                       test_case.delegated_permissions,
                                       /*user_gesture*/ true, GURL(kTestUrl)),
                                   testing::_))
            .WillByDefault(testing::Invoke(forward_callbacks));
      } else {
        EXPECT_CALL(
            *mock_manager(),
            RequestPermissions(rfh,
                               PermissionRequestDescription(
                                   test_case.delegated_permissions,
                                   /*user_gesture*/ true, GURL(kTestUrl)),
                               testing::_))
            .WillOnce(testing::Invoke(forward_callbacks));
      }
    } else {
      // There should be no call to delegate if all overrides are defined.
      EXPECT_CALL(*mock_manager(), RequestPermissionsFromCurrentDocument)
          .Times(0);
    }

    if (test_case.expect_death) {
      GTEST_FLAG_SET(death_test_style, "threadsafe");
      base::MockCallback<RequestsCallback> callback;
      EXPECT_DEATH_IF_SUPPORTED(
          PermissionControllerRequestPermissions(
              rfh,
              PermissionRequestDescription(kTypesToQuery, /*user_gesture*/ true,
                                           GURL(kTestUrl)),
              callback.Get()),
          "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissions(
          rfh,
          PermissionRequestDescription(kTypesToQuery, /*user_gesture*/ true,
                                       GURL(kTestUrl)),
          callback.Get());
    }
  }
}

TEST_F(PermissionControllerImplTest,
       NotifyChangedSubscriptionsCallsOnChangeOnly) {
  using PermissionStatusCallback =
      base::RepeatingCallback<void(PermissionStatus)>;
  GURL kUrl = GURL(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);

  // Setup.
  PermissionStatus sync_status = GetPermissionStatusForWorker(
      PermissionType::BACKGROUND_SYNC,
      /*render_process_host=*/nullptr, kTestOrigin);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::DENIED);

  base::MockCallback<PermissionStatusCallback> geo_callback;
  permission_controller()->SubscribeToPermissionStatusChange(
      PermissionType::GEOLOCATION, nullptr, nullptr, kUrl,
      /*should_include_device_status=*/false, geo_callback.Get());

  base::MockCallback<PermissionStatusCallback> sync_callback;
  permission_controller()->SubscribeToPermissionStatusChange(
      PermissionType::BACKGROUND_SYNC, nullptr, nullptr, kUrl,
      /*should_include_device_status=*/false, sync_callback.Get());

  // Geolocation should change status, so subscriber is updated.
  EXPECT_CALL(geo_callback, Run(PermissionStatus::ASK));
  EXPECT_CALL(sync_callback, Run).Times(0);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::ASK);

  // Callbacks should not be called again because permission status has not
  // changed.
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::BACKGROUND_SYNC, sync_status);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::ASK);
}

TEST_F(PermissionControllerImplTest,
       PermissionsCannotBeOverriddenIfNotOverridable) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_EQ(
      OverrideStatus::kOverrideSet,
      permission_controller()->SetOverrideForDevTools(
          kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::DENIED));

  // Delegate will be called, but prevents override from being set.
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(
      OverrideStatus::kOverrideNotSet,
      permission_controller()->SetOverrideForDevTools(
          kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::ASK));

  PermissionStatus status = GetPermissionStatusForWorker(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
      kTestOrigin);
  EXPECT_EQ(PermissionStatus::DENIED, status);
}

TEST_F(PermissionControllerImplTest,
       GrantPermissionsReturnsStatusesBeingSetIfOverridable) {
  GURL kUrl(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION, PermissionStatus::DENIED);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::MIDI, PermissionStatus::ASK);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::BACKGROUND_SYNC, PermissionStatus::ASK);
  // Delegate will be called, but prevents override from being set.
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::MIDI, testing::_))
      .WillOnce(testing::Return(true));

  // Since one cannot be overridden, none are overridden.
  auto result = permission_controller()->GrantOverridesForDevTools(
      kTestOrigin, {PermissionType::MIDI, PermissionType::GEOLOCATION,
                    PermissionType::BACKGROUND_SYNC});
  EXPECT_EQ(OverrideStatus::kOverrideNotSet, result);

  // Keep original settings as before.
  EXPECT_EQ(PermissionStatus::DENIED,
            GetPermissionStatusForWorker(PermissionType::GEOLOCATION,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
  EXPECT_EQ(
      PermissionStatus::ASK,
      GetPermissionStatusForWorker(
          PermissionType::MIDI, /*render_process_host=*/nullptr, kTestOrigin));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusForWorker(PermissionType::BACKGROUND_SYNC,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));

  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::MIDI, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_manager(), IsPermissionOverridable(
                                   PermissionType::BACKGROUND_SYNC, testing::_))
      .WillOnce(testing::Return(true));
  // If all can be set, overrides will be stored.
  result = permission_controller()->GrantOverridesForDevTools(
      kTestOrigin, {PermissionType::MIDI, PermissionType::GEOLOCATION,
                    PermissionType::BACKGROUND_SYNC});
  EXPECT_EQ(OverrideStatus::kOverrideSet, result);
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForWorker(PermissionType::GEOLOCATION,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
  EXPECT_EQ(
      PermissionStatus::GRANTED,
      GetPermissionStatusForWorker(
          PermissionType::MIDI, /*render_process_host=*/nullptr, kTestOrigin));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusForWorker(PermissionType::BACKGROUND_SYNC,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
}

class PermissionControllerImplWithDelegateTest
    : public content::RenderViewHostTestHarness {
 public:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    std::unique_ptr<TestBrowserContext> browser_context =
        std::make_unique<TestBrowserContext>();

    std::unique_ptr<TestPermissionManager> permission_manager =
        std::make_unique<TestPermissionManager>();
    permission_manager_ = permission_manager.get();
    browser_context->SetPermissionControllerDelegate(
        std::move(permission_manager));

    return browser_context;
  }

  void TearDown() override {
    permission_manager_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const GURL& origin,
      blink::mojom::PermissionsPolicyFeature feature =
          blink::mojom::PermissionsPolicyFeature::kNotFound) {
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (feature != blink::mojom::PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector{*blink::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(origin))},
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", frame_policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, origin);
    return result;
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  TestPermissionManager* permission_manager() const {
    return permission_manager_;
  }

 private:
  raw_ptr<TestPermissionManager> permission_manager_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PermissionControllerImplWithDelegateTest, PermissionPolicyTest) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://example-child.com";

  NavigateAndCommit(GURL(kOrigin1));
  PermissionController* permission_controller =
      GetBrowserContext()->GetPermissionController();
  RenderFrameHost* parent = main_rfh();

  ASSERT_TRUE(parent);

  EXPECT_EQ(PermissionStatus::ASK,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, parent));

  content::RenderFrameHost* child_without_policy =
      AddChildRFH(parent, GURL(kOrigin2));
  ASSERT_TRUE(child_without_policy);

  // A cross-origin iframe without a permission policy has no access to a
  // permission-gated functionality.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, child_without_policy));

  content::RenderFrameHost* child_with_policy =
      AddChildRFH(parent, GURL(kOrigin2),
                  blink::mojom::PermissionsPolicyFeature::kGeolocation);
  ASSERT_TRUE(child_with_policy);

  // The top-level frame has no permission, hence a cross-origin iframe has no
  // permission as well.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, child_without_policy));

  permission_manager()->SetPermissionStatus(GURL(kOrigin1),
                                            PermissionStatus::GRANTED);

  // The top-level frame has granted permission.
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, parent));

  // A cross-origin iframe with a permission policy has full access to a
  // permission-gated functionality as long as the top-level frame has
  // permission.
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, child_with_policy));

  // The frame without a permission policy still has no access.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                PermissionType::GEOLOCATION, child_without_policy));
}
#endif

}  // namespace content
