// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include <cstdlib>
#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
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
    base::OnceCallback<void(const std::vector<PermissionResult>&)>;

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
       const base::OnceCallback<void(const std::vector<PermissionResult>&)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      RequestPermissions,
      (RenderFrameHost * render_frame_host,
       const PermissionRequestDescription& request_description,
       const base::OnceCallback<void(const std::vector<PermissionResult>&)>
           callback),
      (override));
  MOCK_METHOD(bool,
              IsPermissionOverridable,
              (PermissionType,
               base::optional_ref<const url::Origin>,
               base::optional_ref<const url::Origin>),
              (override));
};

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager() = default;
  ~TestPermissionManager() override = default;

  PermissionResult GetPermissionResultForCurrentDocument(
      const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
      RenderFrameHost* render_frame_host,
      bool should_include_device_status) override {
    RenderFrameHost* top_frame = render_frame_host->GetParentOrOuterDocument();
    GURL url;

    if (top_frame) {
      url = top_frame->GetLastCommittedOrigin().GetURL();
    } else {
      url = render_frame_host->GetLastCommittedOrigin().GetURL();
    }

    if (auto it = override_status_.find(url); it != override_status_.end()) {
      return PermissionResult(it->second);
    }

    return PermissionResult(PermissionStatus::ASK);
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
  std::vector<PermissionResult> delegated_results;

  std::vector<PermissionResult> expected_results;
  bool expect_death;
} kTestPermissionRequestCases[] = {
    // No overrides present - all delegated.
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {PermissionResult(PermissionStatus::DENIED),
      PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::GRANTED)},
     {PermissionResult(PermissionStatus::DENIED),
      PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::GRANTED)},
     /*expect_death=*/false},

    // No delegates needed - all overridden.
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC, PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, PermissionStatus::ASK}},
     {},
     {},
     {PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::ASK)},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::BACKGROUND_SYNC, PermissionStatus::DENIED}},
     {PermissionType::GEOLOCATION, PermissionType::MIDI_SYSEX},
     {
         PermissionResult(PermissionStatus::GRANTED),
         PermissionResult(PermissionStatus::ASK),
     },
     {PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::DENIED),
      PermissionResult(PermissionStatus::ASK)},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC, PermissionStatus::DENIED}},
     {PermissionType::MIDI_SYSEX},
     {PermissionResult(PermissionStatus::ASK)},
     {PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::DENIED),
      PermissionResult(PermissionStatus::ASK)},
     /*expect_death=*/false},

    // Too many delegates (causes death).
    {{{PermissionType::GEOLOCATION, PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, PermissionStatus::ASK}},
     {PermissionType::BACKGROUND_SYNC},
     {PermissionResult(PermissionStatus::DENIED),
      PermissionResult(PermissionStatus::GRANTED)},
     // Results don't matter because will die.
     {},
     /*expect_death=*/true},

    // Too few delegates (causes death).
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {PermissionResult(PermissionStatus::GRANTED),
      PermissionResult(PermissionStatus::GRANTED)},
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
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback) {
    permission_controller()->RequestPermissionsFromCurrentDocument(
        render_frame_host, std::move(request_description), std::move(callback));
  }

  void PermissionControllerRequestPermissions(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(const std::vector<PermissionResult>&)> callback) {
    permission_controller()->RequestPermissions(
        render_frame_host, std::move(request_description), std::move(callback));
  }

  PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) {
    return permission_controller()->GetPermissionStatusForWorker(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(permission),
        render_process_host, worker_origin);
  }

  PermissionResult GetPermissionResultForOriginWithoutContext(
      const blink::mojom::PermissionDescriptorPtr& permission,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) {
    return permission_controller()->GetPermissionResultForOriginWithoutContext(
        permission, requesting_origin, embedding_origin);
  }

  BrowserContext* browser_context() { return &browser_context_; }

  MockManagerWithRequests* mock_manager() {
    return static_cast<MockManagerWithRequests*>(
        browser_context_.GetPermissionControllerDelegate());
  }

  OverrideStatus SetPermissionOverrideAndWait(
      const std::optional<url::Origin>& requesting_origin,
      const std::optional<url::Origin>& embedding_origin,
      PermissionType permission,
      PermissionStatus status) {
    base::test::TestFuture<OverrideStatus> future;
    permission_controller()->SetPermissionOverride(
        requesting_origin, embedding_origin, permission, status,
        future.GetCallback());
    return future.Get();
  }

  OverrideStatus GrantPermissionOverridesAndWait(
      const std::optional<url::Origin>& requesting_origin,
      const std::optional<url::Origin>& embedding_origin,
      const std::vector<PermissionType>& permissions) {
    base::test::TestFuture<OverrideStatus> future;
    permission_controller()->GrantPermissionOverrides(
        requesting_origin, embedding_origin, permissions, future.GetCallback());

    return future.Get();
  }

  void ResetPermissionOverridesAndWait() {
    base::test::TestFuture<void> future;
    permission_controller()->ResetPermissionOverrides(future.GetCallback());
    ASSERT_TRUE(future.Wait());
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
    ResetPermissionOverridesAndWait();
    for (const auto& permission_status_pair : test_case.overrides) {
      SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                                   permission_status_pair.first,
                                   permission_status_pair.second);
    }

    // Expect request permission from current document calls if override are
    // missing.
    if (!test_case.delegated_permissions.empty()) {
      auto forward_callbacks = testing::WithArg<2>(
          [&test_case](
              base::OnceCallback<void(const std::vector<PermissionResult>&)>
                  callback) {
            std::move(callback).Run(test_case.delegated_results);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissionsFromCurrentDocument(
                    rfh,
                    PermissionRequestDescription(
                        content::PermissionDescriptorUtil::
                            CreatePermissionDescriptorForPermissionTypes(
                                test_case.delegated_permissions),
                        /*user_gesture*/ true, GURL(kTestUrl)),
                    testing::_))
            .WillByDefault(forward_callbacks);
      } else {
        EXPECT_CALL(*mock_manager(),
                    RequestPermissionsFromCurrentDocument(
                        rfh,
                        PermissionRequestDescription(
                            content::PermissionDescriptorUtil::
                                CreatePermissionDescriptorForPermissionTypes(
                                    test_case.delegated_permissions),
                            /*user_gesture*/ true, GURL(kTestUrl)),
                        testing::_))
            .WillOnce(forward_callbacks);
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
              PermissionRequestDescription(
                  content::PermissionDescriptorUtil::
                      CreatePermissionDescriptorForPermissionTypes(
                          kTypesToQuery),
                  /*user_gesture*/ true),
              callback.Get()),
          "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissionsFromCurrentDocument(
          rfh,
          PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionTypes(kTypesToQuery),
              /*user_gesture*/ true),
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
    ResetPermissionOverridesAndWait();
    for (const auto& permission_status_pair : test_case.overrides) {
      SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                                   permission_status_pair.first,
                                   permission_status_pair.second);
    }

    // Expect request permission call if override are missing.
    if (!test_case.delegated_permissions.empty()) {
      auto forward_callbacks = testing::WithArg<2>(
          [&test_case](
              base::OnceCallback<void(const std::vector<PermissionResult>&)>
                  callback) {
            std::move(callback).Run(test_case.delegated_results);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissions(
                    rfh,
                    PermissionRequestDescription(
                        content::PermissionDescriptorUtil::
                            CreatePermissionDescriptorForPermissionTypes(
                                test_case.delegated_permissions),
                        /*user_gesture*/ true, GURL(kTestUrl)),
                    testing::_))
            .WillByDefault(forward_callbacks);
      } else {
        EXPECT_CALL(*mock_manager(),
                    RequestPermissions(
                        rfh,
                        PermissionRequestDescription(
                            content::PermissionDescriptorUtil::
                                CreatePermissionDescriptorForPermissionTypes(
                                    test_case.delegated_permissions),
                            /*user_gesture*/ true, GURL(kTestUrl)),
                        testing::_))
            .WillOnce(forward_callbacks);
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
              PermissionRequestDescription(
                  content::PermissionDescriptorUtil::
                      CreatePermissionDescriptorForPermissionTypes(
                          kTypesToQuery),
                  /*user_gesture*/ true, GURL(kTestUrl)),
              callback.Get()),
          "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissions(
          rfh,
          PermissionRequestDescription(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionTypes(kTypesToQuery),
              /*user_gesture*/ true, GURL(kTestUrl)),
          callback.Get());
    }
  }
}

TEST_F(PermissionControllerImplTest,
       NotifyChangedSubscriptionsCallsOnChangeOnly) {
  using PermissionResultCallback =
      base::RepeatingCallback<void(PermissionResult)>;
  GURL kUrl = GURL(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);

  // Setup.
  PermissionStatus sync_status = GetPermissionStatusForWorker(
      PermissionType::BACKGROUND_SYNC,
      /*render_process_host=*/nullptr, kTestOrigin);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::GEOLOCATION,
                               PermissionStatus::DENIED);

  base::MockCallback<PermissionResultCallback> geo_callback;
  permission_controller()->SubscribeToPermissionResultChange(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          PermissionType::GEOLOCATION),
      nullptr, nullptr, kUrl,
      /*should_include_device_status=*/false, geo_callback.Get());

  base::MockCallback<PermissionResultCallback> sync_callback;
  permission_controller()->SubscribeToPermissionResultChange(
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          PermissionType::BACKGROUND_SYNC),
      nullptr, nullptr, kUrl,
      /*should_include_device_status=*/false, sync_callback.Get());

  // Geolocation should change status, so subscriber is updated.
  EXPECT_CALL(geo_callback, Run(PermissionResult(PermissionStatus::ASK)));
  EXPECT_CALL(sync_callback, Run).Times(0);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::GEOLOCATION,
                               PermissionStatus::ASK);

  // Callbacks should not be called again because permission status has not
  // changed.
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::BACKGROUND_SYNC, sync_status);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::GEOLOCATION,
                               PermissionStatus::ASK);
}

TEST_F(PermissionControllerImplTest,
       PermissionsCannotBeOverriddenIfNotOverridable) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_EQ(SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                                         PermissionType::GEOLOCATION,
                                         PermissionStatus::DENIED),
            OverrideStatus::kOverrideSet);

  // Delegate will be called, but prevents override from being set.
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_,
                                      testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                                         PermissionType::GEOLOCATION,
                                         PermissionStatus::ASK),
            OverrideStatus::kOverrideNotSet);

  PermissionStatus status = GetPermissionStatusForWorker(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
      kTestOrigin);
  EXPECT_EQ(PermissionStatus::DENIED, status);
}

TEST_F(PermissionControllerImplTest,
       GrantPermissionsReturnsStatusesBeingSetIfOverridable) {
  GURL kUrl(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::GEOLOCATION,
                               PermissionStatus::DENIED);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin, PermissionType::MIDI,
                               PermissionStatus::ASK);
  SetPermissionOverrideAndWait(kTestOrigin, kTestOrigin,
                               PermissionType::BACKGROUND_SYNC,
                               PermissionStatus::ASK);
  // Delegate will be called, but prevents override from being set.
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_,
                                      testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_manager(), IsPermissionOverridable(PermissionType::MIDI,
                                                       testing::_, testing::_))
      .WillOnce(testing::Return(true));

  // Since one cannot be overridden, none are overridden.
  EXPECT_EQ(OverrideStatus::kOverrideNotSet,
            GrantPermissionOverridesAndWait(
                kTestOrigin, kTestOrigin,
                {PermissionType::MIDI, PermissionType::GEOLOCATION,
                 PermissionType::BACKGROUND_SYNC}));

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
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_,
                                      testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_manager(), IsPermissionOverridable(PermissionType::MIDI,
                                                       testing::_, testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::BACKGROUND_SYNC,
                                      testing::_, testing::_))
      .WillOnce(testing::Return(true));
  // If all can be set, overrides will be stored.
  EXPECT_EQ(OverrideStatus::kOverrideSet,
            GrantPermissionOverridesAndWait(
                kTestOrigin, kTestOrigin,
                {PermissionType::MIDI, PermissionType::GEOLOCATION,
                 PermissionType::BACKGROUND_SYNC}));
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

TEST_F(PermissionControllerImplTest, SetOverrideEmbeddingOriginMatters) {
  url::Origin requesting_origin =
      url::Origin::Create(GURL("https://requester.com/"));
  url::Origin embedding_origin_1 =
      url::Origin::Create(GURL("https://embedder1.com/"));
  url::Origin embedding_origin_2 =
      url::Origin::Create(GURL("https://embedder2.com/"));

  // Create distinct overrides because embedding origin matters for
  // STORAGE_ACCESS_GRANT.
  EXPECT_EQ(SetPermissionOverrideAndWait(requesting_origin, embedding_origin_1,
                                         PermissionType::STORAGE_ACCESS_GRANT,
                                         PermissionStatus::GRANTED),
            OverrideStatus::kOverrideSet);

  EXPECT_EQ(SetPermissionOverrideAndWait(requesting_origin, embedding_origin_2,
                                         PermissionType::STORAGE_ACCESS_GRANT,
                                         PermissionStatus::DENIED),
            OverrideStatus::kOverrideSet);

  const blink::mojom::PermissionDescriptorPtr
      storage_access_permission_descriptor = content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::STORAGE_ACCESS_GRANT);

  EXPECT_EQ(GetPermissionResultForOriginWithoutContext(
                storage_access_permission_descriptor, requesting_origin,
                embedding_origin_1)
                .status,
            PermissionStatus::GRANTED);

  // For the STORAGE_ACCESS_GRANT permission, the DENIED status must be masked
  // as ASK (PROMPT) when queried to prevent any attempt at retaliating against
  // users who would reject a prompt.
  EXPECT_EQ(GetPermissionResultForOriginWithoutContext(
                storage_access_permission_descriptor, requesting_origin,
                embedding_origin_2)
                .status,
            PermissionStatus::ASK);

  // Pairs without overrides should return ASK.
  url::Origin no_overrides_origin =
      url::Origin::Create(GURL("https://example.com"));
  EXPECT_EQ(GetPermissionResultForOriginWithoutContext(
                storage_access_permission_descriptor, no_overrides_origin,
                embedding_origin_1)
                .status,
            PermissionStatus::ASK);
  EXPECT_EQ(GetPermissionResultForOriginWithoutContext(
                storage_access_permission_descriptor, requesting_origin,
                no_overrides_origin)
                .status,
            PermissionStatus::ASK);
}

TEST_F(PermissionControllerImplTest, SetOverrideCrashesOnSingleOrigin) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  // Setting overrides should crash if only one origin is provided.
  EXPECT_DEATH_IF_SUPPORTED(
      permission_controller()->SetPermissionOverride(
          kTestOrigin, std::nullopt, PermissionType::GEOLOCATION,
          PermissionStatus::GRANTED, base::DoNothing()),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      permission_controller()->SetPermissionOverride(
          std::nullopt, kTestOrigin, PermissionType::GEOLOCATION,
          PermissionStatus::GRANTED, base::DoNothing()),
      "");
}

TEST_F(PermissionControllerImplTest, GrantOverridesCrashesOnSingleOrigin) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));

  // Granting overrides should crash if only one origin is provided.
  EXPECT_DEATH_IF_SUPPORTED(
      permission_controller()->GrantPermissionOverrides(
          kTestOrigin, std::nullopt, {PermissionType::GEOLOCATION},
          base::DoNothing()),
      "");
  EXPECT_DEATH_IF_SUPPORTED(
      permission_controller()->GrantPermissionOverrides(
          std::nullopt, kTestOrigin, {PermissionType::GEOLOCATION},
          base::DoNothing()),
      "");
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
      network::mojom::PermissionsPolicyFeature feature =
          network::mojom::PermissionsPolicyFeature::kNotFound) {
    network::ParsedPermissionsPolicy frame_policy = {};
    if (feature != network::mojom::PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector{*network::OriginWithPossibleWildcards::FromOrigin(
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

  const auto geolocation_permission_descriptor = content::
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          PermissionType::GEOLOCATION);

  EXPECT_EQ(PermissionStatus::ASK,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, parent));

  content::RenderFrameHost* child_without_policy =
      AddChildRFH(parent, GURL(kOrigin2));
  ASSERT_TRUE(child_without_policy);

  // A cross-origin iframe without a permission policy has no access to a
  // permission-gated functionality.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child_without_policy));

  content::RenderFrameHost* child_with_policy =
      AddChildRFH(parent, GURL(kOrigin2),
                  network::mojom::PermissionsPolicyFeature::kGeolocation);
  ASSERT_TRUE(child_with_policy);

  // The top-level frame has no permission, hence a cross-origin iframe has no
  // permission as well.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child_without_policy));

  permission_manager()->SetPermissionStatus(GURL(kOrigin1),
                                            PermissionStatus::GRANTED);

  // The top-level frame has granted permission.
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, parent));

  // A cross-origin iframe with a permission policy has full access to a
  // permission-gated functionality as long as the top-level frame has
  // permission.
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child_with_policy));

  // The frame without a permission policy still has no access.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child_without_policy));
}
#endif

}  // namespace content
