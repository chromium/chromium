// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/cpp/navigable_contents.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {
namespace {

class ContentServiceBrowserTest : public ContentBrowserTest {
 public:
  ContentServiceBrowserTest() = default;
  ~ContentServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    base::FilePath test_data_path;
    CHECK(base::PathService::Get(DIR_TEST_DATA, &test_data_path));
    embedded_test_server()->ServeFilesFromDirectory(test_data_path);
    CHECK(embedded_test_server()->Start());
  }

 protected:
  content::mojom::NavigableContentsFactory* GetFactory() {
    if (!factory_.is_bound()) {
      auto* connector = BrowserContext::GetConnectorFor(
          shell()->web_contents()->GetBrowserContext());
      connector->Connect(content::mojom::kServiceName,
                         factory_.BindNewPipeAndPassReceiver());
    }
    return factory_.get();
  }

 private:
  mojo::Remote<content::mojom::NavigableContentsFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentServiceBrowserTest);
};

class StopLoadingObserver : public content::NavigableContentsObserver {
 public:
  StopLoadingObserver() = default;
  ~StopLoadingObserver() override = default;

  void CallOnNextStopLoading(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  // content::NavigableContentsObserver:
  void DidStopLoading() override {
    if (callback_)
      std::move(callback_).Run();
  }

  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(StopLoadingObserver);
};

class NavigationObserver : public content::NavigableContentsObserver {
 public:
  NavigationObserver() = default;
  ~NavigationObserver() override = default;

  void CallOnNextNavigation(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  size_t navigations_finished() const { return navigations_finished_; }
  const GURL& last_url() const { return last_url_; }

 private:
  void DidFinishNavigation(
      const GURL& url,
      bool is_main_frame,
      bool is_error_page,
      const net::HttpResponseHeaders* response_headers) override {
    ++navigations_finished_;
    last_url_ = url;
    if (callback_)
      std::move(callback_).Run();
  }

  base::OnceClosure callback_;
  size_t navigations_finished_ = 0;
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(NavigationObserver);
};

class NavigationSuppressedObserver : public content::NavigableContentsObserver {
 public:
  NavigationSuppressedObserver() = default;
  ~NavigationSuppressedObserver() override = default;

  void CallOnNextNavigationSuppression(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  size_t navigations_suppressed() const { return navigations_suppressed_; }
  const GURL& last_url() const { return last_url_; }

 private:
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override {
    ++navigations_suppressed_;
    last_url_ = url;
    if (callback_)
      std::move(callback_).Run();
  }

  base::OnceClosure callback_;
  size_t navigations_suppressed_ = 0;
  GURL last_url_;

  DISALLOW_COPY_AND_ASSIGN(NavigationSuppressedObserver);
};

// Verifies that the embedded Content Service is reachable. Does a basic
// end-to-end sanity check to also verify that a NavigableContents is backed by
// a WebContents instance in the browser.
IN_PROC_BROWSER_TEST_F(ContentServiceBrowserTest, EmbeddedContentService) {
  auto contents = std::make_unique<NavigableContents>(GetFactory());

  const GURL kTestUrl = embedded_test_server()->GetURL("/hello.html");
  StopLoadingObserver observer;
  base::RunLoop loop;
  observer.CallOnNextStopLoading(loop.QuitClosure());
  contents->AddObserver(&observer);
  contents->Navigate(kTestUrl);
  loop.Run();
  contents->RemoveObserver(&observer);
}

IN_PROC_BROWSER_TEST_F(ContentServiceBrowserTest, DidFinishNavigation) {
  auto contents = std::make_unique<NavigableContents>(GetFactory());

  const GURL kTestUrl = embedded_test_server()->GetURL("/hello.html");
  NavigationObserver observer;
  base::RunLoop loop;
  observer.CallOnNextNavigation(loop.QuitClosure());
  contents->AddObserver(&observer);
  contents->Navigate(kTestUrl);

  EXPECT_EQ(0u, observer.navigations_finished());

  loop.Run();
  contents->RemoveObserver(&observer);

  EXPECT_EQ(1u, observer.navigations_finished());
  EXPECT_EQ(kTestUrl, observer.last_url());
}

IN_PROC_BROWSER_TEST_F(ContentServiceBrowserTest, SuppressNavigations) {
  auto params = mojom::NavigableContentsParams::New();
  params->suppress_navigations = true;
  auto contents =
      std::make_unique<NavigableContents>(GetFactory(), std::move(params));

  NavigationObserver navigation_observer;

  NavigationSuppressedObserver suppressed_observer;
  base::RunLoop suppressed_loop;
  suppressed_observer.CallOnNextNavigationSuppression(
      suppressed_loop.QuitClosure());

  contents->AddObserver(&navigation_observer);
  contents->AddObserver(&suppressed_observer);

  // This URL is expected to elicit an automatic navigation on load, which
  // should be suppressed because |suppress_navigations| is |true|.
  const GURL kTestUrl =
      embedded_test_server()->GetURL("/navigate_on_load.html");
  contents->Navigate(kTestUrl);

  EXPECT_EQ(0u, navigation_observer.navigations_finished());
  EXPECT_EQ(0u, suppressed_observer.navigations_suppressed());

  suppressed_loop.Run();

  EXPECT_EQ(1u, navigation_observer.navigations_finished());
  EXPECT_EQ(1u, suppressed_observer.navigations_suppressed());

  EXPECT_EQ(kTestUrl, navigation_observer.last_url());
  EXPECT_EQ(embedded_test_server()->GetURL("/hello.html"),
            suppressed_observer.last_url());

  // Now force a navigation on the same contents. It should complete and we can
  // verify that only two navigations (the initial explicit navigation above,
  // and the one we do here) were completed. This effectively confirms that the
  // scripted navigation to "/hello.html" was fully suppressed.
  const GURL kTestUrl2 = embedded_test_server()->GetURL("/title1.html");
  contents->Navigate(kTestUrl2);

  base::RunLoop navigation_loop;
  navigation_observer.CallOnNextNavigation(navigation_loop.QuitClosure());
  navigation_loop.Run();

  contents->RemoveObserver(&navigation_observer);
  contents->RemoveObserver(&suppressed_observer);

  EXPECT_EQ(2u, navigation_observer.navigations_finished());
  EXPECT_EQ(kTestUrl2, navigation_observer.last_url());
}

}  // namespace
}  // namespace content
