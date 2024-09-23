// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/fake_navigation_policy_provider.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPagePath[] = "/title1.html";
const char kPageTitle[] = "title 1";

}  // namespace

class NavigationPolicyTest : public WebEngineBrowserTest {
 public:
  NavigationPolicyTest() : policy_provider_binding_(&policy_provider_) {
    WebEngineBrowserTest::set_test_server_root(base::FilePath(kTestServerRoot));
  }
  ~NavigationPolicyTest() override = default;

  NavigationPolicyTest(const NavigationPolicyTest&) = delete;
  NavigationPolicyTest& operator=(const NavigationPolicyTest&) = delete;

  void SetUp() override { WebEngineBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    frame_ = FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

    // Spin the loop to allow the Create() request to be handled.
    base::RunLoop().RunUntilIdle();
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_.ptr());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Register a NavigationPolicyProvider to the frame.
    fuchsia::web::NavigationPolicyProviderParams params;
    *params.mutable_main_frame_phases() = fuchsia::web::NavigationPhase::START;
    *params.mutable_subframe_phases() = fuchsia::web::NavigationPhase::REDIRECT;
    frame_->SetNavigationPolicyProvider(std::move(params),
                                        policy_provider_binding_.NewBinding());
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(
        frame_impl_->navigation_policy_handler()->is_provider_connected());
  }

  void TearDownOnMainThread() override {
    frame_ = {};
    WebEngineBrowserTest::TearDownOnMainThread();
  }

 protected:
  FrameForTest frame_;
  FrameImpl* frame_impl_ = nullptr;
  fidl::Binding<fuchsia::web::NavigationPolicyProvider>
      policy_provider_binding_;
  FakeNavigationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_F(NavigationPolicyTest, Proceed) {
  policy_provider_.set_should_abort_navigation(false);

  GURL page_url(embedded_test_server()->GetURL(std::string(kPagePath)));
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame_.navigation_listener().RunUntilUrlAndTitleEquals(page_url, kPageTitle);

  EXPECT_EQ(page_url.spec(), policy_provider_.requested_navigation()->url());
}

IN_PROC_BROWSER_TEST_F(NavigationPolicyTest, Deferred) {
  policy_provider_.set_should_abort_navigation(true);

  GURL page_url(embedded_test_server()->GetURL(std::string(kPagePath)));

  // Make sure the page has had time to load, but has not actually loaded, since
  // we cannot check for the absence of page data.
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();

  // Make sure an up to date NavigationState is used.
  fuchsia::web::NavigationState state;
  state.set_url(page_url.spec());
  frame_.navigation_listener().RunUntilNavigationStateMatches(state);
  auto* current_state = frame_.navigation_listener().current_state();
  EXPECT_TRUE(current_state->has_is_main_document_loaded());
  EXPECT_FALSE(current_state->is_main_document_loaded());

  EXPECT_EQ(page_url.spec(), policy_provider_.requested_navigation()->url());
}
