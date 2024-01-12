// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "content/public/browser/navigation_handle_user_data.h"

#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_contents_observer_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {
class Data : public NavigationHandleUserData<Data> {
 public:
  ~Data() override = default;

  base::WeakPtr<Data> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  const std::string& value() const { return value_; }

 private:
  Data(NavigationHandle& navigation, std::string value) : value_(value) {}
  friend class NavigationHandleUserData<Data>;

  std::string value_;

  base::WeakPtrFactory<Data> weak_ptr_factory_{this};

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(Data);

}  // namespace

class NavigationHandleUserDataBrowserTest : public ContentBrowserTest {
 public:
  ~NavigationHandleUserDataBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* top_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }
};

IN_PROC_BROWSER_TEST_F(NavigationHandleUserDataBrowserTest,
                       SuccessfulNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  TestNavigationManager navigation_manager(web_contents(), url);

  // Start a navigation.
  shell()->LoadURL(url);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  // Attach some data to the navigation.
  Data::CreateForNavigationHandle(*navigation_manager.GetNavigationHandle(),
                                  "data");

  // Check that we can retrieve the data.
  base::WeakPtr<Data> data =
      Data::GetForNavigationHandle(*navigation_manager.GetNavigationHandle())
          ->GetWeakPtr();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->value(), "data");

  // Wait for the navigation to finish.
  bool did_finish_navigation = false;
  NavigationFinishObserver dfn(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation) {
        EXPECT_TRUE(navigation->HasCommitted());
        ASSERT_TRUE(Data::GetForNavigationHandle(*navigation));
        ASSERT_TRUE(data);
        EXPECT_EQ(data->value(), "data");
        did_finish_navigation = true;
      }));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_TRUE(did_finish_navigation);

  // Ensure that the data is deleted after navigation finished.
  EXPECT_FALSE(data);
}

IN_PROC_BROWSER_TEST_F(NavigationHandleUserDataBrowserTest, FailedNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  TestNavigationManager navigation_manager(web_contents(), url);

  // Start a navigation.
  shell()->LoadURL(url);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());

  // Attach some data to the navigation.
  Data::CreateForNavigationHandle(*navigation_manager.GetNavigationHandle(),
                                  "data");

  // Check that we can retrieve the data.
  base::WeakPtr<Data> data =
      Data::GetForNavigationHandle(*navigation_manager.GetNavigationHandle())
          ->GetWeakPtr();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->value(), "data");

  // Wait for DidFinishNavigation event.
  bool did_finish_navigation = false;
  NavigationFinishObserver dfn(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation) {
        EXPECT_TRUE(!navigation->HasCommitted());
        ASSERT_TRUE(Data::GetForNavigationHandle(*navigation));
        ASSERT_TRUE(data);
        EXPECT_EQ(data->value(), "data");
        did_finish_navigation = true;
      }));
  shell()->Stop();
  EXPECT_TRUE(did_finish_navigation);

  // Ensure that the data is deleted after navigation finished.
  EXPECT_FALSE(data);
}

IN_PROC_BROWSER_TEST_F(NavigationHandleUserDataBrowserTest, VeryEarlyAttach) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));

  TestNavigationManager navigation_manager(web_contents(), url);

  int64_t navigation_id;
  base::WeakPtr<Data> data;

  // Expect that the data will be available inside DidStartNavigation and
  // DidFinishNavigation.
  bool did_start_navigation = false;
  NavigationStartObserver dsn(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation) {
        // Attach some data to the navigation.
        Data::CreateForNavigationHandle(*navigation, "data");
        ASSERT_TRUE(Data::GetForNavigationHandle(*navigation));
        EXPECT_EQ(Data::GetForNavigationHandle(*navigation)->value(), "data");
        did_start_navigation = true;
      }));

  bool did_finish_navigation = false;
  NavigationFinishObserver dfn(
      web_contents(),
      base::BindLambdaForTesting([&](NavigationHandle* navigation) {
        EXPECT_EQ(navigation_id, navigation->GetNavigationId());
        ASSERT_TRUE(Data::GetForNavigationHandle(*navigation));
        ASSERT_TRUE(data);
        EXPECT_EQ(data->value(), "data");
        did_finish_navigation = true;
      }));

  // Start a navigation.
  auto* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  web_contents_impl->GetController().LoadURLWithParams(
      NavigationController::LoadURLParams(url));
  auto* navigation =
      web_contents_impl->GetPrimaryFrameTree().root()->navigation_request();
  ASSERT_TRUE(navigation);
  navigation_id = navigation->GetNavigationId();

  // Check that we can retrieve the data.
  data = Data::GetForNavigationHandle(*navigation_manager.GetNavigationHandle())
             ->GetWeakPtr();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->value(), "data");

  // Wait for the navigation to finish.
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_TRUE(did_start_navigation);
  EXPECT_TRUE(did_finish_navigation);

  // Ensure that the data is deleted after navigation finished.
  EXPECT_FALSE(data);
}

}  // namespace content
