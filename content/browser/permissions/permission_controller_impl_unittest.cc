// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_controller_impl.h"

#include <cstdlib>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
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
using RequestsCallback = base::OnceCallback<void(
    const std::vector<blink::mojom::PermissionStatus>&)>;

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
      (const std::vector<PermissionType>& permission,
       RenderFrameHost* render_frame_host,
       bool user_gesture,
       const base::OnceCallback<
           void(const std::vector<blink::mojom::PermissionStatus>&)> callback),
      (override));
  MOCK_METHOD(
      void,
      RequestPermissions,
      (const std::vector<PermissionType>& permission,
       RenderFrameHost* render_frame_host,
       const GURL& requesting_origin,
       bool user_gesture,
       const base::OnceCallback<
           void(const std::vector<blink::mojom::PermissionStatus>&)> callback),
      (override));
  MOCK_METHOD(bool,
              IsPermissionOverridable,
              (PermissionType, const absl::optional<url::Origin>&),
              (override));
};

// Results are defined based on assumption that same types are queried for
// each test case.
const struct {
  std::map<PermissionType, blink::mojom::PermissionStatus> overrides;

  std::vector<PermissionType> delegated_permissions;
  std::vector<blink::mojom::PermissionStatus> delegated_statuses;

  std::vector<blink::mojom::PermissionStatus> expected_results;
  bool expect_death;
} kTestPermissionRequestCases[] = {
    // No overrides present - all delegated.
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {blink::mojom::PermissionStatus::DENIED,
      blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::GRANTED},
     {blink::mojom::PermissionStatus::DENIED,
      blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::GRANTED},
     /*expect_death=*/false},

    // No delegates needed - all overridden.
    {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC,
       blink::mojom::PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, blink::mojom::PermissionStatus::ASK}},
     {},
     {},
     {blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::ASK},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::BACKGROUND_SYNC,
       blink::mojom::PermissionStatus::DENIED}},
     {PermissionType::GEOLOCATION, PermissionType::MIDI_SYSEX},
     {blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::ASK},
     {blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::DENIED,
      blink::mojom::PermissionStatus::ASK},
     /*expect_death=*/false},

    // Some overridden, some delegated.
    {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
      {PermissionType::BACKGROUND_SYNC,
       blink::mojom::PermissionStatus::DENIED}},
     {PermissionType::MIDI_SYSEX},
     {blink::mojom::PermissionStatus::ASK},
     {blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::DENIED,
      blink::mojom::PermissionStatus::ASK},
     /*expect_death=*/false},

    // Too many delegates (causes death).
    {{{PermissionType::GEOLOCATION, blink::mojom::PermissionStatus::GRANTED},
      {PermissionType::MIDI_SYSEX, blink::mojom::PermissionStatus::ASK}},
     {PermissionType::BACKGROUND_SYNC},
     {blink::mojom::PermissionStatus::DENIED,
      blink::mojom::PermissionStatus::GRANTED},
     // Results don't matter because will die.
     {},
     /*expect_death=*/true},

    // Too few delegates (causes death).
    {{},
     {PermissionType::GEOLOCATION, PermissionType::BACKGROUND_SYNC,
      PermissionType::MIDI_SYSEX},
     {blink::mojom::PermissionStatus::GRANTED,
      blink::mojom::PermissionStatus::GRANTED},
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

  ~PermissionControllerImplTest() override {}

  void SetUp() override {
    ON_CALL(*mock_manager(), IsPermissionOverridable)
        .WillByDefault(testing::Return(true));
  }

  PermissionControllerImpl* permission_controller() {
    return permission_controller_.get();
  }

  void PermissionControllerRequestPermissionsFromCurrentDocument(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback) {
    permission_controller()->RequestPermissionsFromCurrentDocument(
        permission, render_frame_host, user_gesture, std::move(callback));
  }

  void PermissionControllerRequestPermissions(
      const std::vector<PermissionType>& permission,
      RenderFrameHost* render_frame_host,
      const url::Origin& requested_origin,
      bool user_gesture,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback) {
    permission_controller()->RequestPermissions(permission, render_frame_host,
                                                requested_origin, user_gesture,
                                                std::move(callback));
  }

  blink::mojom::PermissionStatus GetPermissionStatusForWorker(
      PermissionType permission,
      RenderProcessHost* render_process_host,
      const url::Origin& worker_origin) {
    return permission_controller()->GetPermissionStatusForWorker(
        permission, render_process_host, worker_origin);
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
      auto forward_callbacks = testing::WithArg<3>(
          [&test_case](base::OnceCallback<void(
                           const std::vector<blink::mojom::PermissionStatus>&)>
                           callback) {
            std::move(callback).Run(test_case.delegated_statuses);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissionsFromCurrentDocument(
                    testing::ElementsAreArray(test_case.delegated_permissions),
                    rfh, true, testing::_))
            .WillByDefault(testing::Invoke(forward_callbacks));
      } else {
        EXPECT_CALL(
            *mock_manager(),
            RequestPermissionsFromCurrentDocument(
                testing::ElementsAreArray(test_case.delegated_permissions), rfh,
                true, testing::_))
            .WillOnce(testing::Invoke(forward_callbacks));
      }
    } else {
      // There should be no call to delegate if all overrides are defined.
      EXPECT_CALL(*mock_manager(), RequestPermissionsFromCurrentDocument)
          .Times(0);
    }

    if (test_case.expect_death) {
      ::testing::FLAGS_gtest_death_test_style = "threadsafe";
      base::MockCallback<RequestsCallback> callback;
      EXPECT_DEATH_IF_SUPPORTED(
          PermissionControllerRequestPermissionsFromCurrentDocument(
              kTypesToQuery, rfh,
              /*user_gesture=*/true, callback.Get()),
          "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissionsFromCurrentDocument(
          kTypesToQuery, rfh,
          /*user_gesture=*/true, callback.Get());
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
      auto forward_callbacks = testing::WithArg<4>(
          [&test_case](base::OnceCallback<void(
                           const std::vector<blink::mojom::PermissionStatus>&)>
                           callback) {
            std::move(callback).Run(test_case.delegated_statuses);
            return 0;
          });
      // Regular tests can set expectations.
      if (test_case.expect_death) {
        // Death tests cannot track these expectations but arguments should be
        // forwarded to ensure death occurs.
        ON_CALL(*mock_manager(),
                RequestPermissions(
                    testing::ElementsAreArray(test_case.delegated_permissions),
                    rfh, testing::_, true, testing::_))
            .WillByDefault(testing::Invoke(forward_callbacks));
      } else {
        EXPECT_CALL(*mock_manager(),
                    RequestPermissions(testing::ElementsAreArray(
                                           test_case.delegated_permissions),
                                       rfh, testing::_, true, testing::_))
            .WillOnce(testing::Invoke(forward_callbacks));
      }
    } else {
      // There should be no call to delegate if all overrides are defined.
      EXPECT_CALL(*mock_manager(), RequestPermissionsFromCurrentDocument)
          .Times(0);
    }

    if (test_case.expect_death) {
      ::testing::FLAGS_gtest_death_test_style = "threadsafe";
      base::MockCallback<RequestsCallback> callback;
      EXPECT_DEATH_IF_SUPPORTED(PermissionControllerRequestPermissions(
                                    kTypesToQuery, rfh, testing_origin,
                                    /*user_gesture=*/true, callback.Get()),
                                "");
    } else {
      base::MockCallback<RequestsCallback> callback;
      EXPECT_CALL(callback,
                  Run(testing::ElementsAreArray(test_case.expected_results)));
      PermissionControllerRequestPermissions(kTypesToQuery, rfh, testing_origin,
                                             /*user_gesture=*/true,
                                             callback.Get());
    }
  }
}

TEST_F(PermissionControllerImplTest,
       NotifyChangedSubscriptionsCallsOnChangeOnly) {
  using PermissionStatusCallback =
      base::RepeatingCallback<void(blink::mojom::PermissionStatus)>;
  GURL kUrl = GURL(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);

  // Setup.
  blink::mojom::PermissionStatus sync_status = GetPermissionStatusForWorker(
      PermissionType::BACKGROUND_SYNC,
      /*render_process_host=*/nullptr, kTestOrigin);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::DENIED);

  base::MockCallback<PermissionStatusCallback> geo_callback;
  permission_controller()->SubscribePermissionStatusChange(
      PermissionType::GEOLOCATION, nullptr, nullptr, kUrl, geo_callback.Get());

  base::MockCallback<PermissionStatusCallback> sync_callback;
  permission_controller()->SubscribePermissionStatusChange(
      PermissionType::BACKGROUND_SYNC, nullptr, nullptr, kUrl,
      sync_callback.Get());

  // Geolocation should change status, so subscriber is updated.
  EXPECT_CALL(geo_callback, Run(blink::mojom::PermissionStatus::ASK));
  EXPECT_CALL(sync_callback, Run).Times(0);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::ASK);

  // Callbacks should not be called again because permission status has not
  // changed.
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::BACKGROUND_SYNC, sync_status);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::ASK);
}

TEST_F(PermissionControllerImplTest,
       PermissionsCannotBeOverriddenIfNotOverridable) {
  url::Origin kTestOrigin = url::Origin::Create(GURL(kTestUrl));
  EXPECT_EQ(OverrideStatus::kOverrideSet,
            permission_controller()->SetOverrideForDevTools(
                kTestOrigin, PermissionType::GEOLOCATION,
                blink::mojom::PermissionStatus::DENIED));

  // Delegate will be called, but prevents override from being set.
  EXPECT_CALL(*mock_manager(),
              IsPermissionOverridable(PermissionType::GEOLOCATION, testing::_))
      .WillOnce(testing::Return(false));
  EXPECT_EQ(OverrideStatus::kOverrideNotSet,
            permission_controller()->SetOverrideForDevTools(
                kTestOrigin, PermissionType::GEOLOCATION,
                blink::mojom::PermissionStatus::ASK));

  blink::mojom::PermissionStatus status = GetPermissionStatusForWorker(
      PermissionType::GEOLOCATION, /*render_process_host=*/nullptr,
      kTestOrigin);
  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, status);
}

TEST_F(PermissionControllerImplTest,
       GrantPermissionsReturnsStatusesBeingSetIfOverridable) {
  GURL kUrl(kTestUrl);
  url::Origin kTestOrigin = url::Origin::Create(kUrl);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::GEOLOCATION,
      blink::mojom::PermissionStatus::DENIED);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::MIDI, blink::mojom::PermissionStatus::ASK);
  permission_controller()->SetOverrideForDevTools(
      kTestOrigin, PermissionType::BACKGROUND_SYNC,
      blink::mojom::PermissionStatus::ASK);
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
  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionStatusForWorker(PermissionType::GEOLOCATION,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
  EXPECT_EQ(
      blink::mojom::PermissionStatus::ASK,
      GetPermissionStatusForWorker(
          PermissionType::MIDI, /*render_process_host=*/nullptr, kTestOrigin));
  EXPECT_EQ(blink::mojom::PermissionStatus::ASK,
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
  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            GetPermissionStatusForWorker(PermissionType::GEOLOCATION,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
  EXPECT_EQ(
      blink::mojom::PermissionStatus::GRANTED,
      GetPermissionStatusForWorker(
          PermissionType::MIDI, /*render_process_host=*/nullptr, kTestOrigin));
  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            GetPermissionStatusForWorker(PermissionType::BACKGROUND_SYNC,
                                         /*render_process_host=*/nullptr,
                                         kTestOrigin));
}

}  // namespace content
