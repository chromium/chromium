// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_loader_factory_manager.h"

#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/shell/browser/shell_extension_loader.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

class URLLoaderFactoryManagerBrowserTest : public ShellApiTest,
                                           public content::WebContentsDelegate {
 public:
  URLLoaderFactoryManagerBrowserTest() = default;
  ~URLLoaderFactoryManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  };

  void TearDownOnMainThread() override {
    tab1_.reset();
    tab2_.reset();
    ShellApiTest::TearDownOnMainThread();
  }

  const Extension* InstallContentScriptsExtension(
      const std::string& content_scripts_manifest_declaration) {
    const char kManifestTemplate[] = R"(
        {
          "name": "ContentScriptsTest",
          "version": "1.0",
          "manifest_version": 2,
          %s
        } )";
    dir_.WriteManifest(base::StringPrintf(
        kManifestTemplate, content_scripts_manifest_declaration.c_str()));
    dir_.WriteFile(FILE_PATH_LITERAL("content_script.css"), "");
    dir_.WriteFile(FILE_PATH_LITERAL("content_script.js"), "");

    ShellExtensionLoader loader(browser_context());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      extension_ = loader.LoadExtension(dir_.UnpackedPath());
    }
    return extension_;
  }

  // Returns whether the class-under-test (URLLoaderFactoryManager) thinks that
  // the test extension (installed by individual test cases via
  // InstallContentScriptsExtension) may inject content scripts into the
  // foo_frame frame in tab1 (see SetUpFrameTree for a list of available test
  // frames).
  //
  // The optional |url| argument may be used to simulate a ready-to-commit
  // scenario where the frame's last committed URL may differ from the |url|
  // that a pending navigation is ready to commit.
  bool DoContentScriptsMatch_Tab1_FooFrame(GURL url = GURL("http://foo.com")) {
    return DoContentScriptsMatchNavigatingFrame(tab1_fooFrame(), url);
  }

  // Like DoContentScriptsMatch_Tab1_FooFrame, but for foo_about_blank_frame.
  bool DoContentScriptsMatch_Tab1_FooBlankFrame(
      GURL url = GURL(url::kAboutBlankURL)) {
    return DoContentScriptsMatchNavigatingFrame(tab1_fooBlankFrame(), url);
  }

  // Like DoContentScriptsMatch_Tab1_FooFrame, but for bar_frame.
  bool DoContentScriptsMatch_Tab1_BarFrame(GURL url = GURL("http://bar.com")) {
    return DoContentScriptsMatchNavigatingFrame(tab1_barFrame(), url);
  }

  // Like DoContentScriptsMatch_Tab1_FooFrame, but for bar_about_blank_frame.
  bool DoContentScriptsMatch_Tab1_BarBlankFrame(
      GURL url = GURL(url::kAboutBlankURL)) {
    return DoContentScriptsMatchNavigatingFrame(tab1_barBlankFrame(), url);
  }

  // Like DoContentScriptsMatch_Tab1_FooFrame, but for bar_about_blank_frame1 in
  // tab2.
  bool DoContentScriptsMatch_Tab2_BarBlankFrame1(
      GURL url = GURL(url::kAboutBlankURL)) {
    return DoContentScriptsMatchNavigatingFrame(tab2_barBlankFrame1(), url);
  }

  // Like DoContentScriptsMatch_Tab1_FooFrame, but for bar_about_blank_frame2 in
  // tab2.
  bool DoContentScriptsMatch_Tab2_BarBlankFrame2(
      GURL url = GURL(url::kAboutBlankURL)) {
    return DoContentScriptsMatchNavigatingFrame(tab2_barBlankFrame2(), url);
  }

  // SetUpFrameTree sets up the following frame tree(s) that are used by all the
  // URLLoaderFactoryManagerBrowserTest.ContentScriptMatching_* tests.
  //
  // tab1_:
  //   foo_frame
  //   +-foo_about_blank_frame
  //   +-bar_frame
  //     +-bar_about_blank_frame <---\
  //                                 |
  // tab2_:                          |^opener
  //   bar_about_blank_frame1--------/
  //   +-bar_about_blank_frame2
  void SetUpFrameTree() {
    GURL foo_url = embedded_test_server()->GetURL("foo.com", "/empty.html");
    GURL bar_url = embedded_test_server()->GetURL("bar.com", "/empty.html");
    GURL blank_url = GURL(url::kAboutBlankURL);
    url::Origin foo_origin = url::Origin::Create(foo_url);
    url::Origin bar_origin = url::Origin::Create(bar_url);

    tab1_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser_context()));
    tab1_->SetDelegate(this);
    ASSERT_TRUE(content::NavigateToURL(tab1_.get(), foo_url));

    AddFrame(tab1_fooFrame(), "fooBlankFrame");

    AddFrame(tab1_fooFrame(), "barFrame");
    ASSERT_TRUE(NavigateIframeToURL(tab1_.get(), "barFrame", bar_url));

    AddFrame(tab1_barFrame(), "barBlankFrame");

    content::WebContentsAddedObserver new_tab_observer;
    ASSERT_TRUE(content::ExecJs(tab1_barBlankFrame(),
                                "window.open('', 'barBlankFrame1');"));
    new_tab_observer.GetWebContents();

    AddFrame(tab2_barBlankFrame1(), "barBlankFrame2");

    EXPECT_EQ(foo_origin, tab1_fooFrame()->GetLastCommittedOrigin());
    EXPECT_EQ(foo_origin, tab1_fooBlankFrame()->GetLastCommittedOrigin());
    EXPECT_EQ(bar_origin, tab1_barFrame()->GetLastCommittedOrigin());
    EXPECT_EQ(bar_origin, tab1_barBlankFrame()->GetLastCommittedOrigin());
    EXPECT_EQ(bar_origin, tab2_barBlankFrame1()->GetLastCommittedOrigin());
    EXPECT_EQ(bar_origin, tab2_barBlankFrame2()->GetLastCommittedOrigin());

    EXPECT_EQ(foo_url, tab1_fooFrame()->GetLastCommittedURL());
    EXPECT_EQ(blank_url, tab1_fooBlankFrame()->GetLastCommittedURL());
    EXPECT_EQ(bar_url, tab1_barFrame()->GetLastCommittedURL());
    EXPECT_EQ(blank_url, tab1_barBlankFrame()->GetLastCommittedURL());
    EXPECT_EQ(blank_url, tab2_barBlankFrame1()->GetLastCommittedURL());
    EXPECT_EQ(blank_url, tab2_barBlankFrame2()->GetLastCommittedURL());
  }

 private:
  // WebContentsDelegate overrides:
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override {
    DCHECK_EQ(tab1_.get(), source);
    DCHECK(new_contents);
    tab2_ = std::move(new_contents);
  }

  bool DoContentScriptsMatchNavigatingFrame(
      content::RenderFrameHost* navigating_frame,
      const GURL& navigation_target) {
    return URLLoaderFactoryManager::DoContentScriptsMatchNavigatingFrame(
        *extension_, navigating_frame, navigation_target);
  }

  void AddFrame(content::RenderFrameHost* parent,
                const std::string& subframe_id) {
    const char* kScriptTemplate = R"(
            var bar_frame = document.createElement('iframe');
            bar_frame.id = $1;
            document.body.appendChild(bar_frame);
        )";
    ASSERT_TRUE(content::ExecJs(
        parent, content::JsReplace(kScriptTemplate, subframe_id)));
  }

  content::RenderFrameHost* tab1_fooFrame() {
    DCHECK(tab1_);
    return tab1_->GetMainFrame();
  }

  content::RenderFrameHost* tab1_fooBlankFrame() {
    DCHECK(tab1_);
    DCHECK_LT(1u, tab1_->GetAllFrames().size());
    return tab1_->GetAllFrames()[1];
  }

  content::RenderFrameHost* tab1_barFrame() {
    DCHECK(tab1_);
    DCHECK_LT(2u, tab1_->GetAllFrames().size());
    return tab1_->GetAllFrames()[2];
  }

  content::RenderFrameHost* tab1_barBlankFrame() {
    DCHECK(tab1_);
    DCHECK_LT(3u, tab1_->GetAllFrames().size());
    return tab1_->GetAllFrames()[3];
  }

  content::RenderFrameHost* tab2_barBlankFrame1() {
    DCHECK(tab2_);
    return tab2_->GetMainFrame();
  }

  content::RenderFrameHost* tab2_barBlankFrame2() {
    DCHECK(tab2_);
    DCHECK_LT(1u, tab1_->GetAllFrames().size());
    return tab2_->GetAllFrames()[1];
  }

  // Populated by SetUpFrameTree (during test setup / in SetUpOnMainThread).
  std::unique_ptr<content::WebContents> tab1_;
  std::unique_ptr<content::WebContents> tab2_;

  // Populated by InstallContentScriptsExtension (called by individual tests).
  TestExtensionDir dir_;
  const Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_ChainTraversalForBar) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": true,
        "match_about_blank": true,
        "matches": ["http://bar.com/*"],
        "js": ["content_script.js"]
      }] )");
  ASSERT_TRUE(extension);

  // Matching should consider parent/opener chain.
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooBlankFrame());
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_BarFrame());
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_BarBlankFrame());
  EXPECT_TRUE(DoContentScriptsMatch_Tab2_BarBlankFrame1());
  EXPECT_TRUE(DoContentScriptsMatch_Tab2_BarBlankFrame2());
}

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_ChainTraversalForFoo) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": true,
        "match_about_blank": true,
        "matches": ["http://foo.com/*"],
        "js": ["content_script.js"]
      }] )");
  ASSERT_TRUE(extension);

  // Matching should consider parent/opener chain.
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_FooFrame());
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_FooBlankFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarBlankFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab2_BarBlankFrame1());
  EXPECT_FALSE(DoContentScriptsMatch_Tab2_BarBlankFrame2());
}

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_NoMatchingOfAboutBlank) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": true,
        "match_about_blank": false,
        "matches": ["http://bar.com/*"],
        "js": ["content_script.js"]
      }] )");
  ASSERT_TRUE(extension);

  // In absence of "match_about_blank", parent/opener chain should not be
  // considered (and matching against about:blank should fail).
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooBlankFrame());
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_BarFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarBlankFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab2_BarBlankFrame1());
  EXPECT_FALSE(DoContentScriptsMatch_Tab2_BarBlankFrame2());
}

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_NotAllFrames) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": false,
        "match_about_blank": true,
        "matches": ["http://foo.com/*", "http://bar.com/*"],
        "js": ["content_script.js"]
      }] )");
  ASSERT_TRUE(extension);

  // Main frame should be matched.
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_FooFrame());

  // Subframe should not be matched (even though the patterns in the manifest do
  // match bar.com).
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarFrame());
}

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_NotYetCommittedURL) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": true,
        "match_about_blank": true,
        "matches": ["http://matching.com/*"],
        "js": ["content_script.js"]
      }] )");
  ASSERT_TRUE(extension);

  // Content scripts should match, even though baz.com has not yet committed in
  // the frames (i.e. GetLastCommittedOrigin() in the frames is different -
  // either foo.com or bar.com).
  GURL matching_url("http://matching.com");
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_FooFrame(matching_url));
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_BarFrame(matching_url));
  EXPECT_TRUE(DoContentScriptsMatch_Tab1_FooBlankFrame(matching_url));

  // Content scripts should not match, since other.com is not covered by the
  // extension manifest.
  GURL other_url("http://other.com");
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooFrame(other_url));
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarFrame(other_url));
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooBlankFrame(other_url));
}

IN_PROC_BROWSER_TEST_F(URLLoaderFactoryManagerBrowserTest,
                       ContentScriptMatching_CssIsIgnored) {
  SetUpFrameTree();
  ASSERT_FALSE(::testing::Test::HasFailure());

  const Extension* extension = InstallContentScriptsExtension(R"(
      "content_scripts": [{
        "all_frames": true,
        "match_about_blank": false,
        "matches": ["http://bar.com/*"],
        "css": ["content_script.css"]
      }] )");
  ASSERT_TRUE(extension);

  // Only Javascript should result in a match.
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_FooFrame());
  EXPECT_FALSE(DoContentScriptsMatch_Tab1_BarFrame());
}

}  // namespace extensions
