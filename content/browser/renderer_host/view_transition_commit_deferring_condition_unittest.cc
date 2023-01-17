// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/view_transition_commit_deferring_condition.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/render_document_feature.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {
class ViewTransitionCommitDeferringConditionTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override { RenderViewHostImplTestHarness::SetUp(); }

  void AddOptIn() {
    auto* opt_in_state =
        ViewTransitionOptInState::GetOrCreateForCurrentDocument(
            main_test_rfh());
    EXPECT_EQ(opt_in_state->same_origin_opt_in(),
              blink::mojom::ViewTransitionSameOriginOptIn::kDisabled);
    opt_in_state->set_same_origin_opt_in(
        blink::mojom::ViewTransitionSameOriginOptIn::kEnabled);
  }

 private:
  base::test::ScopedFeatureList view_transition_feature_{
      blink::features::kViewTransitionOnNavigation};
};

// This test verifies that we can determine whether or not to create a
// ViewTransitionCommitDeferringCondition before the navigation request state is
// in the WILL_PROCESS_RESPONSE state. This checks the tentative origin at
// request time, instead of origin to commit, since the latter is not
// appropriate to get before the request state is in WILL_PROCESS_RESPONSE
// state. This is verified by DCHECKs.
TEST_F(ViewTransitionCommitDeferringConditionTest,
       CreateCommitDeferringConditionBeforeProcessResponse) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      blink::features::kViewTransitionOnNavigation));

  // Create a cross-origin navigation. Note that this will count as cross
  // origin, since there has been no other navigation yet.
  const GURL kFirstUrl("http://example.com/");
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kFirstUrl, contents());
  navigation->Start();
  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->IsInPrimaryMainFrame());

  AddOptIn();

  EXPECT_LT(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);

  EXPECT_NE(request->frame_tree_node()
                ->current_frame_host()
                ->GetLastCommittedOrigin(),
            request->GetTentativeOriginAtRequestTime());

  // We don't create a deferring condition for cross-origin navigations.
  auto deferring_condition =
      ViewTransitionCommitDeferringCondition::MaybeCreate(*request);
  EXPECT_FALSE(deferring_condition);

  // Commit the navigation so we're now on the example.com origin.
  navigation->Commit();

  // Now create a same-origin navigation
  const GURL kSecondUrl("http://example.com/foo.html");

  navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(kSecondUrl, contents());
  navigation->Start();
  request = main_test_rfh()->frame_tree_node()->navigation_request();
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->IsInPrimaryMainFrame());

  EXPECT_LT(request->state(), NavigationRequest::WILL_PROCESS_RESPONSE);

  AddOptIn();

  EXPECT_EQ(request->frame_tree_node()
                ->current_frame_host()
                ->GetLastCommittedOrigin(),
            request->GetTentativeOriginAtRequestTime());

  // Same origin navigation should create a deferring condition
  deferring_condition =
      ViewTransitionCommitDeferringCondition::MaybeCreate(*request);
  EXPECT_TRUE(deferring_condition);
}

}  // namespace content
