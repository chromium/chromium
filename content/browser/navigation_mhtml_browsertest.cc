// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
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
    EXPECT_NE(-1, base::WriteFile(file_path, document.data(), document.size()));
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

  // TODO(arthursonzogni): When the document is not found, the navigation never
  // commit, even if we wait longer. Find out why.
  EXPECT_FALSE(iframe_navigation_observer.has_committed());
  EXPECT_FALSE(iframe_navigation_observer.is_error());
  EXPECT_EQ(GURL(), sub_document->GetLastCommittedURL());
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
  EXPECT_EQ(iframe_url(0), GURL("about:blank"));
  EXPECT_EQ(iframe_url(1), GURL());  // TODO(arthursonzogni): Why is this empty?
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

  auto console_delegate = std::make_unique<ConsoleObserverDelegate>(
      web_contents(),
      base::StringPrintf(
          "Blocked script execution in '%s' because the document's frame "
          "is sandboxed and the 'allow-scripts' permission is not set.",
          mhtml_url.spec().c_str()));
  web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  console_delegate->Wait();

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

  auto console_delegate = std::make_unique<ConsoleObserverDelegate>(
      web_contents(),
      base::StringPrintf(
          "Blocked script execution in '%s' because the document's frame "
          "is sandboxed and the 'allow-scripts' permission is not set.",
          mhtml_url.spec().c_str()));
  web_contents()->SetDelegate(console_delegate.get());

  EXPECT_TRUE(NavigateToURL(shell(), mhtml_url));
  console_delegate->Wait();

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

  EXPECT_EQ(GURL(""), sub_document->GetLastCommittedURL());
  EXPECT_FALSE(iframe_navigation.has_committed());
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

}  // namespace content
