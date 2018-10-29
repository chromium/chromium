// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/resource_fetcher.h"

#include <stdint.h>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

static const int kMaxWaitTimeMs = 5000;

class FetcherDelegate {
 public:
  FetcherDelegate() : completed_(false), timed_out_(false) {
    // Start a repeating timer waiting for the download to complete.  The
    // callback has to be a static function, so we hold on to our instance.
    FetcherDelegate::instance_ = this;
    StartTimer();
  }

  virtual ~FetcherDelegate() {}

  ResourceFetcher::Callback NewCallback() {
    return base::BindOnce(&FetcherDelegate::OnURLFetchComplete,
                          base::Unretained(this));
  }

  virtual void OnURLFetchComplete(const WebURLResponse& response,
                                  const std::string& data) {
    response_ = response;
    data_ = data;
    completed_ = true;
    timer_.Stop();
    if (!timed_out_)
      quit_task_.Run();
  }

  bool completed() const { return completed_; }
  bool timed_out() const { return timed_out_; }

  std::string data() const { return data_; }
  const WebURLResponse& response() const { return response_; }

  // Wait for the request to complete or timeout.
  void WaitForResponse() {
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
    quit_task_ = runner->QuitClosure();
    runner->Run();
  }

  void StartTimer() {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kMaxWaitTimeMs),
                 this, &FetcherDelegate::TimerFired);
  }

  void TimerFired() {
    ASSERT_FALSE(completed_);

    timed_out_ = true;
    if (!completed_)
      quit_task_.Run();
    FAIL() << "fetch timed out";
  }

  static FetcherDelegate* instance_;

 private:
  base::OneShotTimer timer_;
  bool completed_;
  bool timed_out_;
  WebURLResponse response_;
  std::string data_;
  base::Closure quit_task_;
};

FetcherDelegate* FetcherDelegate::instance_ = nullptr;

class EvilFetcherDelegate : public FetcherDelegate {
 public:
  ~EvilFetcherDelegate() override {}

  void SetFetcher(ResourceFetcher* fetcher) { fetcher_.reset(fetcher); }

  void OnURLFetchComplete(const WebURLResponse& response,
                          const std::string& data) override {
    FetcherDelegate::OnURLFetchComplete(response, data);

    // Destroy the ResourceFetcher here.  We are testing that upon returning
    // to the ResourceFetcher that it does not crash.  This must be done after
    // calling FetcherDelegate::OnURLFetchComplete, since deleting the fetcher
    // invalidates |response| and |data|.
    fetcher_.reset();
  }

 private:
  std::unique_ptr<ResourceFetcher> fetcher_;
};

class ResourceFetcherTests : public ContentBrowserTest {
 public:
  ResourceFetcherTests() : render_view_routing_id_(MSG_ROUTING_NONE) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  void SetUpOnMainThread() override {
    render_view_routing_id_ =
        shell()->web_contents()->GetRenderViewHost()->GetRoutingID();
  }

  RenderView* GetRenderView() {
    return RenderView::FromRoutingID(render_view_routing_id_);
  }

  void ResourceFetcherDownloadOnRenderer(const GURL& url) {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();

    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(200, delegate->response().HttpStatusCode());
    std::string text = delegate->data();
    EXPECT_TRUE(text.find("Basic html test.") != std::string::npos);
  }

  void ResourceFetcherRedirectOnRenderer(const GURL& url,
                                         const GURL& final_url) {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();

    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(200, delegate->response().HttpStatusCode());
    EXPECT_EQ(final_url.spec(), delegate->response().Url().GetString().Utf8());
    std::string text = delegate->data();
    EXPECT_TRUE(text.find("Basic html test.") != std::string::npos);
  }

  void ResourceFetcher404OnRenderer(const GURL& url) {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();

    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(404, delegate->response().HttpStatusCode());
  }

  void ResourceFetcherDidFailOnRenderer() {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    // Try to fetch a page on a site that doesn't exist.
    GURL url("http://localhost:1339/doesnotexist");
    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();

    // When we fail, we still call the Delegate callback but we pass in empty
    // values.
    EXPECT_TRUE(delegate->completed());
    EXPECT_TRUE(delegate->response().IsNull());
    EXPECT_EQ(std::string(), delegate->data());
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherTimeoutOnRenderer(const GURL& url) {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());
    fetcher->SetTimeout(base::TimeDelta());

    delegate->WaitForResponse();

    // When we timeout, we still call the Delegate callback but we pass in empty
    // values.
    EXPECT_TRUE(delegate->completed());
    EXPECT_TRUE(delegate->response().IsNull());
    EXPECT_EQ(std::string(), delegate->data());
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherDeletedInCallbackOnRenderer(const GURL& url) {
    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<EvilFetcherDelegate> delegate(new EvilFetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());
    fetcher->SetTimeout(base::TimeDelta());
    delegate->SetFetcher(fetcher.release());

    delegate->WaitForResponse();
    EXPECT_FALSE(delegate->timed_out());
  }

  void ResourceFetcherPost(const GURL& url) {
    const char* kBody = "Really nifty POST body!";

    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->SetMethod("POST");
    fetcher->SetBody(kBody);
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();
    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(200, delegate->response().HttpStatusCode());
    EXPECT_EQ(kBody, delegate->data());
  }

  void ResourceFetcherSetHeader(const GURL& url) {
    const char* kHeader = "Rather boring header.";

    blink::WebLocalFrame* frame =
        GetRenderView()->GetWebView()->MainFrame()->ToWebLocalFrame();

    std::unique_ptr<FetcherDelegate> delegate(new FetcherDelegate);
    std::unique_ptr<ResourceFetcher> fetcher(ResourceFetcher::Create(url));
    fetcher->SetHeader("header", kHeader);
    fetcher->Start(frame, blink::mojom::RequestContextType::INTERNAL,
                   RenderFrame::FromWebFrame(frame)->GetURLLoaderFactory(),
                   TRAFFIC_ANNOTATION_FOR_TESTS, delegate->NewCallback());

    delegate->WaitForResponse();
    ASSERT_TRUE(delegate->completed());
    EXPECT_EQ(200, delegate->response().HttpStatusCode());
    EXPECT_EQ(kHeader, delegate->data());
  }

  int32_t render_view_routing_id_;
};

// Test a fetch from the test server.
// If this flakes, use http://crbug.com/51622.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherDownload) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  GURL url(embedded_test_server()->GetURL("/simple_page.html"));

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcherDownloadOnRenderer,
                 base::Unretained(this), url));
}

// Test if ResourceFetcher can handle server redirects correctly.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherRedirect) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  GURL final_url(embedded_test_server()->GetURL("/simple_page.html"));
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcherRedirectOnRenderer,
                 base::Unretained(this), url, final_url));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcher404) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Test 404 response.
  GURL url = embedded_test_server()->GetURL("/thisfiledoesntexist.html");

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcher404OnRenderer,
                 base::Unretained(this), url));
}

// If this flakes, use http://crbug.com/51622.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherDidFail) {
  // Need to spin up the renderer.
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcherDidFailOnRenderer,
                 base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherTimeout) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url(embedded_test_server()->GetURL("/slow?1"));

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcherTimeoutOnRenderer,
                 base::Unretained(this), url));
}

IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherDeletedInCallback) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Grab a page that takes at least 1 sec to respond, but set the fetcher to
  // timeout in 0 sec.
  GURL url(embedded_test_server()->GetURL("/slow?1"));

  PostTaskToInProcessRendererAndWait(base::Bind(
      &ResourceFetcherTests::ResourceFetcherDeletedInCallbackOnRenderer,
      base::Unretained(this), url));
}

// Test that ResourceFetchers can handle POSTs.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherPost) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Grab a page that echos the POST body.
  GURL url(embedded_test_server()->GetURL("/echo"));

  PostTaskToInProcessRendererAndWait(base::Bind(
      &ResourceFetcherTests::ResourceFetcherPost, base::Unretained(this), url));
}

// Test that ResourceFetchers can set headers.
IN_PROC_BROWSER_TEST_F(ResourceFetcherTests, ResourceFetcherSetHeader) {
  // Need to spin up the renderer to same-site URL.
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Grab a page that echos the POST body.
  GURL url(embedded_test_server()->GetURL("/echoheader?header"));

  PostTaskToInProcessRendererAndWait(
      base::Bind(&ResourceFetcherTests::ResourceFetcherSetHeader,
                 base::Unretained(this), url));
}

}  // namespace content
