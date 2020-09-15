// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Tests about navigations to MHTML archives.
class NavigationMhtmlBrowserTest : public ContentBrowserTest {
 public:
  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* main_frame_host() {
    return web_contents()->GetFrameTree()->root()->current_frame_host();
  }

 protected:
  void SetUpOnMainThread() final {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Helper class: Build MHTML documents easily in tests.
class MhtmlArchive {
 public:
  MhtmlArchive() = default;
  ~MhtmlArchive() {
    base::ScopedAllowBlockingForTesting allow_blocking_;
    EXPECT_TRUE(file_directory_.Delete());
  }

  void AddResource(const std::string content) {
    content_ += "\n--MHTML_BOUNDARY\n" + content;
  }

  void AddHtmlDocument(const GURL& url,
                       const std::string headers,
                       const std::string body) {
    const char* document_template =
        "Content-Type: text/html\n"
        "Content-Location: $1\n"
        "$2"
        "\n"
        "$3";
    AddResource(base::ReplaceStringPlaceholders(
        document_template, {url.spec(), headers, body}, nullptr));
  }

  void AddHtmlDocument(const GURL& url, const std::string body) {
    AddHtmlDocument(url, "" /* headers */, body);
  }

  // Writes the MHTML archive into a file and returns its URL.
  const GURL Write(const std::string& file) {
    const char* document_header =
        "From: The chromium developers\n"
        "Subject: <the subject>\n"
        "Date: Mon, May 27 2019 11:55:42 GMT+0200\n"
        "MIME-Version: 1.0\n"
        "Content-Type: multipart/related;"
        "              boundary=\"MHTML_BOUNDARY\";"
        "              type=\"text/html\"\n";
    std::string document = document_header + content_ + "\n--MHTML_BOUNDARY--";

    // MHTML uses carriage return before every new lines.
    base::ReplaceChars(document, "\n", "\r\n", &document);

    base::ScopedAllowBlockingForTesting allow_blocking_;
    EXPECT_TRUE(file_directory_.CreateUniqueTempDir());
    base::FilePath file_path = file_directory_.GetPath().AppendASCII(file);
    EXPECT_TRUE(base::WriteFile(file_path, document));
    return net::FilePathToFileURL(file_path);
  }

 private:
  base::ScopedTempDir file_directory_;
  std::string content_;

  DISALLOW_COPY_AND_ASSIGN(MhtmlArchive);
};

}  // namespace

// An MHTML document with an iframe. The iframe's document is found in the
// archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"http://example.com/found.html\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("http://example.com/found.html"),
                                "<iframe></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  NavigationHandleObserver iframe_navigation(
      web_contents(), GURL("http://example.com/found.html"));
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  // |is_mhtml_document| is confusing. It always returns false for subframe.
  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_FALSE(sub_document->is_mhtml_document());

  // When the iframe's content is loaded from the MHTML archive, a successful
  // commit using the provided URL happens, even if the resource wasn't loaded
  // from this URL initially.
  EXPECT_EQ(GURL("http://example.com/found.html"),
            sub_document->GetLastCommittedURL());
  EXPECT_TRUE(iframe_navigation.has_committed());
  EXPECT_FALSE(iframe_navigation.is_error());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// An MHTML document with an iframe. The iframe's document is not found in the
// archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeNotFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"http://example.com/not_found.html\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  NavigationHandleObserver iframe_navigation_observer(
      web_contents(), GURL("http://example.com/not_found.html"));
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  // |is_mhtml_document| is confusing. It always returns false for subframe.
  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_FALSE(sub_document->is_mhtml_document());

  // This should commit as a failed navigation, but the browser side doesn't
  // have enough information to make that determination. On the renderer side,
  // there's no existing way to turn `CommitNavigation()` into
  // `CommitFailedNavigation()`.
  // TODO(https://crbug.com/1112965): Fix this by implementing a MHTML
  // URLLoaderFactory; then failure to find the resource can use the standard
  // error handling path.
  EXPECT_TRUE(iframe_navigation_observer.has_committed());
  EXPECT_FALSE(iframe_navigation_observer.is_error());
  EXPECT_EQ(GURL("http://example.com/not_found.html"),
            sub_document->GetLastCommittedURL());
}

// An MHTML document with an iframe using a data-URL. The data-URL is not
// defined in the MHTML archive.
// TODO(https://crbug.com/967307): Enable this test. It currently reaches a
// DCHECK or timeout in release mode.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeDataUrlNotFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"data:text/html,<iframe></iframe>\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  RenderFrameHostImpl* main_document = main_frame_host();

  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL("data:text/html,<iframe></iframe>"),
            sub_document->GetLastCommittedURL());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// An MHTML document with an iframe using a data-URL. The data-URL IS defined in
// the MHTML archive, but isn't used, per https://crbug.com/969696.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeDataUrlFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"data:text/html,<iframe></iframe>\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("data:text/html,<iframe></iframe>"),
                                "no iframes");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  RenderFrameHostImpl* main_document = main_frame_host();

  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL("data:text/html,<iframe></iframe>"),
            sub_document->GetLastCommittedURL());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// An iframe uses its srcdoc attribute and the about:srcdoc is not defined in
// the MHTML archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeAboutSrcdocNoFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe srcdoc=\"<iframe></iframe>\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_TRUE(sub_document->GetLastCommittedURL().IsAboutSrcdoc());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// An iframe uses its srcdoc attribute and the about:srcdoc IS defined in
// the MHTML archive. Its content is NEVER loaded from the MHTML archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeAboutSrcdocFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe srcdoc=\"<iframe></iframe>\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("about:srcdoc'"), "no iframe");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_TRUE(sub_document->GetLastCommittedURL().IsAboutSrcdoc());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// An MHTML document with an iframe loading the about:blank document. The
// about:blank resource is not defined in the MHTML archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeAboutBlankNotFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://example.com"),
                                "<iframe src=\"about:blank\"></iframe>"
                                // Note: this is actually treated as a
                                // same-document navigation!
                                "<iframe src=\"about:blank#fragment\"></iframe>"
                                "<iframe src=\"about:blank?query\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(3u, main_document->child_count());
  auto iframe_url = [main_document](int index) {
    return main_document->child_at(index)
        ->current_frame_host()
        ->GetLastCommittedURL();
  };

  // about:blank in MHTML has some very unusual behavior. When navigating to
  // about:blank in the context of a MHTML archive, the renderer-side MHTML
  // handler actually attempts to look up the resource for about:blank<...>" in
  // the MHTML archive.
  //
  // Prior to https://crrev.com/c/2335323, failing to find the resource in the
  // MHTML archive usually led to the commit being silently dropped (see
  // `IframeNotFound` and `IframeContentIdNotFound`). However, about:blank
  // behaved differently, due to a special case in frame_loader.cc's
  // `ShouldNavigate()` for URLs that will load as an empty document.
  //
  // However, after https://crrev.com/c/23335323, loading about:blank without a
  // corresponding resource in the MHTML archive will be treated as loading
  // static data rather than loading an empty document. This affects the timing
  // of load completion; loading an empty document synchronously completes
  // during `CommitNavigation()`, while loading static data (even if the data is
  // empty) completes "later".
  EXPECT_EQ(iframe_url(0), GURL("about:blank"));
  // Note: unlike the other two subframe navigations, this navigation actually
  // succeeds as a same-document navigation...
  // Note 2: this same-document navigation is performed asynchronously. Prior to
  // https://crrev.com/c/23335323, the test would consider the page as loaded
  // before the fragment navigation completed, resulting in an empty last
  // committed URL.
  EXPECT_EQ(iframe_url(1), GURL("about:blank#fragment"));
  EXPECT_EQ(iframe_url(2), GURL("about:blank?query"));
}

// An MHTML document with an iframe loading the about:blank document AND the
// about:blank document is a resource of the MHTML archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeAboutBlankFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://example.com"),
                                "<iframe src=\"about:blank\"></iframe>");
  mhtml_archive.AddHtmlDocument(
      GURL(url::kAboutBlankURL),
      "<iframe src=\"http://example.com/found.html\">/iframe>");
  mhtml_archive.AddHtmlDocument(GURL("http://example.com/found.html"), "");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* about_blank_document =
      main_document->child_at(0)->current_frame_host();
  // about:blank is loaded from the archive, so it has an iframe.
  // See https://crbug.com/969667
  ASSERT_EQ(1u, about_blank_document->child_count());
}

// An MHTML document with an iframe trying to load a javascript URL.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest,
                       IframeJavascriptUrlNotFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"javascript:console.log('test')\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(base::StringPrintf(
      "Blocked script execution in '%s' because the document's frame "
      "is sandboxed and the 'allow-scripts' permission is not set.",
      mhtml_url.spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  console_observer.Wait();

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL(), sub_document->GetLastCommittedURL());
}

// An MHTML document with an iframe trying to load a javascript URL. The
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeJavascriptUrlFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<iframe src=\"javascript:console.log('test')\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("javascript:console.log('test')"),
                                "<iframe></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern(base::StringPrintf(
      "Blocked script execution in '%s' because the document's frame "
      "is sandboxed and the 'allow-scripts' permission is not set.",
      mhtml_url.spec().c_str()));

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  console_observer.Wait();

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();
  EXPECT_EQ(GURL(), sub_document->GetLastCommittedURL());

  EXPECT_EQ(0u, sub_document->child_count());
}

// Load iframe with the content-ID scheme. The resource is found in the MHTML
// archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeContentIdFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://example.com"),
                                "<iframe src=\"cid:iframe\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("http://example.com/found.html"),
                                "Content-ID: <iframe>\n", "<iframe></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  NavigationHandleObserver iframe_navigation(web_contents(),
                                             GURL("cid:iframe"));
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  EXPECT_EQ(GURL("cid:iframe"), sub_document->GetLastCommittedURL());
  EXPECT_TRUE(iframe_navigation.has_committed());
  EXPECT_FALSE(iframe_navigation.is_error());

  // Check the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  EXPECT_EQ(1u, sub_document->child_count());
}

// Load iframe with the content-ID scheme. The resource is not found in the
// MHTML archive.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, IframeContentIdNotFound) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://example.com"),
                                "<iframe src=\"cid:iframe\"></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  NavigationHandleObserver iframe_navigation(web_contents(),
                                             GURL("cid:iframe"));
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  // This should commit as a failed navigation, but the browser side doesn't
  // have enough information to make that determination. On the renderer side,
  // there's no existing way to turn `CommitNavigation()` into
  // `CommitFailedNavigation()`.
  // TODO(https://crbug.com/1112965): Fix this by implementing a MHTML
  // URLLoaderFactory; then failure to find the resource can use the standard
  // error handling path.
  EXPECT_EQ(GURL("cid:iframe"), sub_document->GetLastCommittedURL());
  EXPECT_TRUE(iframe_navigation.has_committed());
  EXPECT_FALSE(iframe_navigation.is_error());
}

// Tests Content-Security-Policy: frame-ancestors enforcement in MHTML
// subframes. It isn't enforced currently.
// See https://crbug.com/969711.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, CspFrameAncestor) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com/main"),
      "<iframe src=\"http://example.com/subframe\"></iframe>");
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com/subframe"),
      "Content-Security-Policy: frame-ancestors 'none'\n", "<iframe></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_frame = main_frame_host();
  ASSERT_EQ(1u, main_frame->child_count());
  RenderFrameHostImpl* sub_frame =
      main_frame->child_at(0)->current_frame_host();

  // Currently, frame-ancestors is not enforced. See https://crbug.com/969711.
  // Check that the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  ASSERT_EQ(1u, sub_frame->child_count());
}

IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest,
                       SameDocumentNavigationWhileLoading) {
  // Load a MHTML archive normally so there's a renderer process for file://.
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://example.com/main"),
                                "<p>Hello world!</p>");
  const GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  const RenderProcessHost* const rph = main_frame_host()->GetProcess();

  // Navigate to another MHTML archive which will reuse the same renderer.
  MhtmlArchive mhtml_archive2;
  mhtml_archive2.AddHtmlDocument(GURL("http://example.com/main2"),
                                 "<p>Hello world again!</p>");
  const GURL mhtml_url2 = mhtml_archive2.Write("index2.mhtml");

  TestNavigationManager manager(web_contents(), mhtml_url2);
  shell()->LoadURL(mhtml_url2);

  EXPECT_TRUE(manager.WaitForResponse());
  // The new navigation should not have committed yet.
  EXPECT_EQ(mhtml_url, main_frame_host()->GetLastCommittedURL());

  // Make sure it actually picked the same process.
  NavigationRequest* request =
      NavigationRequest::From(manager.GetNavigationHandle());
  EXPECT_EQ(rph, request->GetRenderFrameHost()->GetProcess());

  // Delay the response body from being received by the renderer.
  mojo::ScopedDataPipeConsumerHandle consumer;
  mojo::ScopedDataPipeProducerHandle producer;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/* options */ nullptr, &producer, &consumer));
  using std::swap;
  swap(request->mutable_response_body_for_testing(), consumer);

  // Resume the navigation, which should send a |CommitNavigation()| to the
  // renderer.
  manager.ResumeNavigation();

  // Archive loading is split into two phases: first, the entire response body
  // is read and parsed into an MHTML archive by |MHTMLBodyLoaderClient|, and
  // then the renderer commits the response. Since the data pipe for the
  // response body was swapped out above, the renderer should not have committed
  // a navigation to |mhtml_url2|.
  // Note: Ideally, this should resume the navigation and wait for a signal that
  // the renderer is attempting to read the response body. Unfortunately, no
  // such signal exsts. As-is, this check is imperfect.
  EXPECT_EQ(mhtml_url, main_frame_host()->GetLastCommittedURL());
  EXPECT_TRUE(web_contents()->IsLoading());

  // While archive loading is still in progress and nothing has been committed,
  // trigger a same-document navigation.
  url::Replacements<char> replacements;
  replacements.SetRef("fragment", url::Component(0, strlen("fragment")));
  const GURL mhtml_url_with_fragment =
      mhtml_url.ReplaceComponents(replacements);
  // TODO(dcheng): Using NavigateToURL() here seems to cause the test to hang.
  // Figure out why.
  shell()->LoadURL(mhtml_url_with_fragment);

  // The same-document navigation should cancel MHTML loading. On the browser
  // side, this can be observed by waiting for the peer handle to be closed by
  // the renderer.
  base::RunLoop run_loop;
  mojo::SimpleWatcher watcher(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
  watcher.Watch(
      producer.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindLambdaForTesting(
          [&](MojoResult result, const mojo::HandleSignalsState& state) {
            EXPECT_EQ(MOJO_RESULT_OK, result);
            EXPECT_TRUE(state.peer_closed());
            run_loop.Quit();
          }));
  run_loop.Run();

  WaitForLoadStop(web_contents());
  EXPECT_EQ(mhtml_url_with_fragment, main_frame_host()->GetLastCommittedURL());
}

// Check RenderFrameHostImpl::is_mhtml_document() stays true after same-document
// navigation in MHTML document.
// Regression test for https://crbug.com/1126391
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest,
                       SameDocumentNavigationPreservesMhtmlFlag) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://a.com/a"), "");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  EXPECT_TRUE(main_frame_host()->is_mhtml_document());
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(main_frame_host()->GetLastCommittedURL().spec() + "#foo")));
  EXPECT_TRUE(main_frame_host()->is_mhtml_document());
}

// Check RenderFrameHostImpl::is_mhtml_document() is correctly set for history
// navigation to MHTML document. It should continue to work when restored from
// the BackForwardCache.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest,
                       BackNavigationPreservesMhtmlFlag) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://a.com/a"), "");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  EXPECT_TRUE(main_frame_host()->is_mhtml_document());
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));
  EXPECT_FALSE(main_frame_host()->is_mhtml_document());
  web_contents()->GetController().GoBack();
  WaitForLoadStop(web_contents());
  EXPECT_TRUE(main_frame_host()->is_mhtml_document());
}

}  // namespace content
