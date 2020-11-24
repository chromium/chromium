// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "content/public/test/browser_test.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/fake_navigation_policy_provider.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/navigation_policy_handler.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPagePath[] = "/title1.html";
const char kPageTitle[] = "title 1";

}  // namespace

class NavigationPolicyTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  NavigationPolicyTest() : policy_provider_binding_(&policy_provider_) {
    cr_fuchsia::WebEngineBrowserTest::set_test_server_root(
        base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~NavigationPolicyTest() override = default;

  NavigationPolicyTest(const NavigationPolicyTest&) = delete;
  NavigationPolicyTest& operator=(const NavigationPolicyTest&) = delete;

  void SetUp() override { cr_fuchsia::WebEngineBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    frame_ptr_ =
        cr_fuchsia::WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_ptr_);
    frame_ptr_->GetNavigationController(navigation_controller_.NewRequest());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Register a NavigationPolicyProvider to the frame.
    fuchsia::web::NavigationPolicyProviderParams params;
    *params.mutable_main_frame_phases() = fuchsia::web::NavigationPhase::START;
    *params.mutable_subframe_phases() = fuchsia::web::NavigationPhase::REDIRECT;
    frame_ptr_->SetNavigationPolicyProvider(
        std::move(params), policy_provider_binding_.NewBinding());
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(
        frame_impl_->navigation_policy_handler()->is_provider_connected());
  }

 protected:
  fuchsia::web::FramePtr frame_ptr_;
  FrameImpl* frame_impl_;
  fidl::Binding<fuchsia::web::NavigationPolicyProvider>
      policy_provider_binding_;
  FakeNavigationPolicyProvider policy_provider_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;
};

IN_PROC_BROWSER_TEST_F(NavigationPolicyTest, Proceed) {
  policy_provider_.set_should_abort_navigation(false);

  GURL page_url(embedded_test_server()->GetURL(std::string(kPagePath)));
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPageTitle);

  EXPECT_EQ(page_url.spec(), policy_provider_.requested_navigation()->url());
}

IN_PROC_BROWSER_TEST_F(NavigationPolicyTest, Deferred) {
  policy_provider_.set_should_abort_navigation(true);

  GURL page_url(embedded_test_server()->GetURL(std::string(kPagePath)));

  // Make sure the page has had time to load, but has not actually loaded, since
  // we cannot check for the absence of page data.
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();

  // Make sure an up to date NavigationState is used.
  fuchsia::web::NavigationState state;
  state.set_url(page_url.spec());
  navigation_listener_.RunUntilNavigationStateMatches(state);
  auto* current_state = navigation_listener_.current_state();
  EXPECT_TRUE(current_state->has_is_main_document_loaded());
  EXPECT_FALSE(current_state->is_main_document_loaded());

  EXPECT_EQ(page_url.spec(), policy_provider_.requested_navigation()->url());
}
