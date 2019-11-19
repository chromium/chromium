// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
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
  network::ResourceResponseHead response;
  response.headers = info.headers;
  response.headers->GetMimeType(&response.mime_type);
  params->client->OnReceiveResponse(response);

  std::string body = "some data\r\n";
  uint32_t bytes_written = body.size();
  mojo::DataPipe data_pipe(body.size());
  CHECK_EQ(MOJO_RESULT_OK,
           data_pipe.producer_handle->WriteData(
               body.data(), &bytes_written, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
  params->client->OnStartLoadingResponseBody(
      std::move(data_pipe.consumer_handle));

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

// This is a browser test because test_runner has a PrerendererClient
// implementation, and the purpose of this test is to ensure that content_shell
// does not crash when prerender elements are encountered with no Prererering
// implementation supplied to WebKit.

// TODO(gavinp,jochen): This browser_test depends on there not being a
// prerendering client and prerendering platform provided by the test_shell.
// But both will exist when we use content_shell to run web tests. We must
// then add a mechanism to start content_shell without these, or else this
// test is not very interesting.
const char kPrerenderNoCrashPage[] = "/prerender/prerender-no-crash.html";
IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, PrerenderNoCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kPrerenderNoCrashPage);

  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

}  // namespace content
