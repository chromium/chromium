// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {
namespace {

constexpr char kAsyncScriptThatAbortsOnEndPage[] =
    "/webkit/async_script_abort_on_end.html";

constexpr char k400AbortOnEndUrl[] = "http://url.handled.by.abort.on.end/400";

bool AbortOnEndInterceptor(URLLoaderInterceptor::RequestParams* params) {
  if (params->url_request.url.spec() != k400AbortOnEndUrl)
    return false;

  std::string headers =
      "HTTP/1.1 400 This is not OK\n"
      "Content-type: text/plain\n";
  net::HttpResponseInfo info;
  info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  auto response = network::mojom::URLResponseHead::New();
  response->headers = info.headers;
  response->headers->GetMimeType(&response->mime_type);

  std::string body = "some data\r\n";
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  CHECK_EQ(mojo::CreateDataPipe(body.size(), producer_handle, consumer_handle),
           MOJO_RESULT_OK);
  CHECK_EQ(MOJO_RESULT_OK,
           producer_handle->WriteAllData(base::as_byte_span(body)));
  // Ok to ignore `actually_written_bytes` because of `...ALL_OR_NONE`.
  params->client->OnReceiveResponse(std::move(response),
                                    std::move(consumer_handle), std::nullopt);

  params->client->OnComplete(
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_ABORTED));
  return true;
}

}  // namespace

using WebKitBrowserTest = ContentBrowserTest;

// This is a browser test because it is hard to reproduce reliably in a
// web test without races. http://crbug.com/75604 deals with a request
// for an async script which gets data in the response and immediately
// after aborts. This test creates that condition, and it is passed
// if chrome does not crash.

IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, AbortOnEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  URLLoaderInterceptor interceptor(base::BindRepeating(&AbortOnEndInterceptor));
  GURL url = embedded_test_server()->GetURL(kAsyncScriptThatAbortsOnEndPage);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  // If you are seeing this test fail, please strongly investigate the
  // possibility that http://crbug.com/75604 and
  // https://bugs.webkit.org/show_bug.cgi?id=71122 have reverted before
  // marking this as flakey.
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// This is a browser test because the test_runner framework holds
// onto a Document* reference that blocks this reproduction from
// destroying the Document, so it is not a use after free unless
// you don't have test_runner loaded.

// TODO(gavinp): remove this browser_test if we can get good web test
// coverage of the same issue.
const char kXsltBadImportPage[] = "/webkit/xslt-bad-import.html";
IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, XsltBadImport) {
  ASSERT_TRUE(embedded_test_server()->Start());
  URLLoaderInterceptor interceptor(base::BindRepeating(&AbortOnEndInterceptor));
  GURL url = embedded_test_server()->GetURL(kXsltBadImportPage);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// This is a content_browsertests because the purpose of this test is to ensure
// that content_shell does not crash when <link rel=prerender> elements are
// encountered with no prerendering (NoStatePrefetch) implementation supplied by
// embedders.
IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, PrerenderNoCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/prerender/prerender-no-crash.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

}  // namespace content
