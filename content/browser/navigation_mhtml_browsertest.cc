// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/features.h"
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
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
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

  MhtmlArchive(const MhtmlArchive&) = delete;
  MhtmlArchive& operator=(const MhtmlArchive&) = delete;

  ~MhtmlArchive() {
    base::ScopedAllowBlockingForTesting allow_blocking_;
    EXPECT_TRUE(file_directory_.Delete());
  }

  void AddResource(const std::string content) {
    content_ += "\n--MHTML_BOUNDARY\n" + content;
  }

  void AddResource(const GURL& url,
                   const std::string mime_type,
                   const std::string headers,
                   const std::string body) {
    const char* document_template =
        "Content-Type: $1\n"
        "Content-Location: $2\n"
        "$3"
        "\n"
        "$4";
    AddResource(base::ReplaceStringPlaceholders(
        document_template, {mime_type, url.spec(), headers, body}, nullptr));
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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

  // This should commit as a failed navigation, but the browser side doesn't
  // have enough information to make that determination. On the renderer side,
  // there's no existing way to turn `CommitNavigation()` into
  // `CommitFailedNavigation()`.
  // TODO(crbug.com/40143262): Fix this by implementing a MHTML
  // URLLoaderFactory; then failure to find the resource can use the standard
  // error handling path.
  EXPECT_TRUE(iframe_navigation_observer.has_committed());
  EXPECT_FALSE(iframe_navigation_observer.is_error());
  EXPECT_EQ(GURL("http://example.com/not_found.html"),
            sub_document->GetLastCommittedURL());
}

// An MHTML document with an iframe using a data-URL. The data-URL is not
// defined in the MHTML archive.
// TODO(crbug.com/40629273): Enable this test. It currently reaches a
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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  // TODO(arthursonzogni): This should be true here.
  EXPECT_FALSE(about_blank_document->is_mhtml_document());

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
  ASSERT_TRUE(console_observer.Wait());

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  EXPECT_TRUE(main_document->is_mhtml_document());

  // The |sub_document| is the initial empty document.
  EXPECT_FALSE(sub_document->is_mhtml_document());
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
  ASSERT_TRUE(console_observer.Wait());

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  EXPECT_TRUE(main_document->is_mhtml_document());

  // The |sub_document| is the initial empty document.
  EXPECT_FALSE(sub_document->is_mhtml_document());
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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

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

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

  // This should commit as a failed navigation, but the browser side doesn't
  // have enough information to make that determination. On the renderer side,
  // there's no existing way to turn `CommitNavigation()` into
  // `CommitFailedNavigation()`.
  // TODO(crbug.com/40143262): Fix this by implementing a MHTML
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

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(1u, main_document->child_count());
  RenderFrameHostImpl* sub_document =
      main_document->child_at(0)->current_frame_host();

  EXPECT_TRUE(main_document->is_mhtml_document());
  EXPECT_TRUE(sub_document->is_mhtml_document());

  // Currently, frame-ancestors is not enforced. See https://crbug.com/969711.
  // Check that the iframe is properly loaded. EvalJs("document.body.innerHTML")
  // can't be used, because javascript is disabled. Instead, check it was able
  // to load an iframe.
  ASSERT_EQ(1u, sub_document->child_count());
}

// Tests CSP embedded enforcement blocking an iframes.
// Regression test for https://crbug.com/1112965
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, CSPEmbeddedEnforcement) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://a.com"),
      "<iframe csp=\"sandbox\" src=\"http://a.com/\"></iframe>"
      "<iframe csp=\"sandbox\" src=\"http://b.com/\"></iframe>"
      "<iframe csp=\"sandbox\" src=\"http://b.com/allow\"></iframe>");
  mhtml_archive.AddHtmlDocument(GURL("http://a.com/"), "");
  mhtml_archive.AddHtmlDocument(GURL("http://b.com/"), "");
  mhtml_archive.AddHtmlDocument(GURL("http://b.com/allow"), "Allow-CSP-From: *",
                                "");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  ASSERT_EQ(3u, main_document->child_count());
  RenderFrameHostImpl* rfh_1 = main_document->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_2 = main_document->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_3 = main_document->child_at(0)->current_frame_host();

  // Same-origin without Allow-CSP-From:* => response allowed.
  EXPECT_FALSE(rfh_1->IsErrorDocument());

  // Cross-origin without Allow-CSP-From:* => response blocked;
  // TODO(crbug.com/40143262) Add support for CSPEE in MHTML documents.
  // An error page should be displayed here.
  EXPECT_FALSE(rfh_2->IsErrorDocument());

  // Cross-origin with Allow-CSP-From:* => response allowed.
  EXPECT_FALSE(rfh_3->IsErrorDocument());
}

IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest,
                       SameDocumentNavigationWhileLoading) {
  if (ShouldCreateNewHostForAllFrames() &&
      ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
    GTEST_SKIP() << "When RenderDocument + navigation queueing is enabled, the "
                    "same-document navigation won't cancel the cross-document "
                    "navigation";
  }

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
            mojo::CreateDataPipe(/* options */ nullptr, producer, consumer));
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
  GURL::Replacements replacements;
  replacements.SetRefStr("fragment");
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

IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, SandboxedIframe) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL("http://a.com"), "", R"(
    <iframe src="http://a.com/unsandboxed.html"        ></iframe>
    <iframe src="http://a.com/sandboxed.html"   sandbox></iframe>
  )");
  mhtml_archive.AddHtmlDocument(GURL("http://a.com/sandboxed.html"), "");
  mhtml_archive.AddHtmlDocument(GURL("http://a.com/unsandboxed.html"), "");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* rfh_main = main_frame_host();
  ASSERT_EQ(2u, rfh_main->child_count());
  RenderFrameHostImpl* rfh_unsandboxed =
      rfh_main->child_at(0)->current_frame_host();
  RenderFrameHostImpl* rfh_sandboxed =
      rfh_main->child_at(1)->current_frame_host();

  auto strict_sandbox = network::mojom::WebSandboxFlags::kAll;
  auto default_mhtml_sandbox =
      ~network::mojom::WebSandboxFlags::kPopups &
      ~network::mojom::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;

  EXPECT_EQ(default_mhtml_sandbox, rfh_main->active_sandbox_flags());
  EXPECT_EQ(default_mhtml_sandbox, rfh_unsandboxed->active_sandbox_flags());
  EXPECT_EQ(strict_sandbox, rfh_sandboxed->active_sandbox_flags());
}

// Regression test for https://crbug.com/1155862.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, DataIframe) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://127.0.0.1/starte.html"), "",
      R"( <iframe src="http://8.8.8.8/test.html"></iframe>
          <iframe src="data:text/html,blah1"></iframe>
          <iframe src="about:blank?foo=123"></iframe> )");
  mhtml_archive.AddHtmlDocument(GURL("http://8.8.8.8/test.html"), "", R"(
          <iframe src="data:text/html,blah2"></iframe>
          <iframe src="about:blank?foo=123"></iframe> )");
  mhtml_archive.AddHtmlDocument(GURL("about:blank?foo=123"), "", "foo");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  // The main test verification is that the navigation below succeeds (without
  // crashing in NavigationRequest::GetOriginForURLLoaderFactory).
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  // All MHTML frames should have an opaque origin.
  shell()->web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](RenderFrameHost* frame) {
        EXPECT_TRUE(frame->GetLastCommittedOrigin().opaque())
            << "frame->GetLastCommittedURL() = "
            << frame->GetLastCommittedURL();
      });
}

// Regression test for https://crbug.com/1168249.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, PreloadedTextTrack) {
  // The test uses a cross-site subframe, so any HTTP requests that reach the
  // NetworkService will have `network::ResourceRequest::request_initiator` with
  // a tuple (or precursor tuple in case of opaque origins expected for MHTML
  // documents) that is incompatible with `request_initiator_origin_lock` in
  // `network::mojom::URLLoaderFactoryParams`.
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://main.com/main.html"), "",
      R"( <iframe src="http://subframe.com/subframe.html"></iframe> )");
  mhtml_archive.AddHtmlDocument(
      GURL("http://subframe.com/subframe.html"), "",
      R"( <link rel="preload" href="http://resource.com/track" as="track"> )");
  mhtml_archive.AddResource(GURL("http://resource.com/track"), "text/vtt", "",
                            "fake text track body");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  // The main verification is that ResourceFetcher::StartLoad didn't reach
  // NOTREACHED assertion (against HTTP resource loads triggered from MHTML
  // documents). To detect such NOTREACHED (via renderer crash) it is sufficient
  // for the test to wait for DidStopLoading notification (which is done
  // underneath NavigateToURL called above).
}

// MHTML document with a base URL of |kUnreachableWebDataURL| should not be
// treated as an error page.
IN_PROC_BROWSER_TEST_F(NavigationMhtmlBrowserTest, ErrorBaseURL) {
  NavigationController& controller = web_contents()->GetController();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(web_contents())
                            ->GetPrimaryFrameTree()
                            .root();

  // Prepare an MHTML document with the base URL set to the error page URL.
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(GURL(kUnreachableWebDataURL), "foo");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  // Navigate to the MHTML document.
  FrameNavigateParamsCapturer params_capturer(root);
  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  params_capturer.Wait();

  // Check that the RenderFrameHost, NavigationRequest and NavigationEntry all
  // agree that the document is not an error page.
  RenderFrameHostImpl* main_document = main_frame_host();
  EXPECT_FALSE(main_document->IsErrorDocument());
  EXPECT_FALSE(params_capturer.is_error_page());
  EXPECT_NE(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());
}

class NavigationMhtmlFencedFrameBrowserTest
    : public NavigationMhtmlBrowserTest {
 public:
  NavigationMhtmlFencedFrameBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}}}, /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationMhtmlFencedFrameBrowserTest,
                       MhtmlCannotCreateFencedFrame) {
  MhtmlArchive mhtml_archive;
  mhtml_archive.AddHtmlDocument(
      GURL("http://example.com"),
      "<fencedframe src=\"http://example.com/found.html\"></fencedframe>");
  mhtml_archive.AddHtmlDocument(GURL("http://example.com/found.html"),
                                "<iframe></iframe>");
  GURL mhtml_url = mhtml_archive.Write("index.mhtml");

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));

  RenderFrameHostImpl* main_document = main_frame_host();
  EXPECT_TRUE(main_document->is_mhtml_document());
  // Ensure nothing was created for the fencedframe element. Only a single
  // RenderFrameHost, the `main_document`, should exist.
  int num_documents = 0;
  main_document->ForEachRenderFrameHost(
      [&](RenderFrameHostImpl* rfh) { num_documents++; });
  EXPECT_EQ(1, num_documents);
}

}  // namespace content
