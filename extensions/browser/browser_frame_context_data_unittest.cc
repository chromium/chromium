// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_frame_context_data.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extensions_test.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {

namespace {
const std::string kGoogleUrl = "https://google.com";
}

class BrowserFrameContextDataTest : public ExtensionsTest {
 public:
  BrowserFrameContextDataTest() = default;
  ~BrowserFrameContextDataTest() override = default;

  BrowserFrameContextDataTest(const BrowserFrameContextDataTest&) = delete;
  BrowserFrameContextDataTest& operator=(const BrowserFrameContextDataTest&) =
      delete;

 protected:
  void SetUp() override {
    ExtensionsTest::SetUp();

    // Set up the opener WebContents.
    scoped_refptr<content::SiteInstance> site_instance =
        content::SiteInstance::CreateForURL(browser_context(), google_url_);
    opener_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context(), site_instance);
    SetLastCommittedUrlAndOrigin(opener_web_contents_.get(), google_url_);

    // Set up the child WebContents.
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context(), site_instance);
    auto* web_contents_tester =
        content::WebContentsTester::For(web_contents_.get());
    web_contents_tester->SetOpener(opener_web_contents_.get());
    SetLastCommittedUrlAndOrigin(web_contents_.get(), google_url_);
  }

  void TearDown() override {
    web_contents_.reset();
    opener_web_contents_.reset();
    ExtensionsTest::TearDown();
  }

  content::RenderFrameHost* GetRenderFrameHost(
      content::WebContents* web_contents) {
    return web_contents->GetPrimaryMainFrame();
  }

  void SetLastCommittedUrlAndOrigin(content::WebContents* web_contents,
                                    const GURL& url) {
    auto* web_contents_tester = content::WebContentsTester::For(web_contents);
    web_contents_tester->NavigateAndCommit(url);
  }

  content::WebContents* web_contents() { return web_contents_.get(); }
  content::WebContents* opener_web_contents() {
    return opener_web_contents_.get();
  }

  std::unique_ptr<content::WebContents> opener_web_contents_;
  std::unique_ptr<content::WebContents> web_contents_;

  const GURL google_url_ = GURL(kGoogleUrl);
};

TEST_F(BrowserFrameContextDataTest, Clone) {
  auto data = std::make_unique<BrowserFrameContextData>(
      GetRenderFrameHost(opener_web_contents()));
  std::unique_ptr<FrameContextData> cloned_data = data->CloneFrameContextData();

  EXPECT_EQ(data->GetLocalParentOrOpener(),
            cloned_data->GetLocalParentOrOpener());
  EXPECT_EQ(data->GetUrl(), cloned_data->GetUrl());
  EXPECT_EQ(data->GetOrigin(), cloned_data->GetOrigin());
  EXPECT_EQ(data->GetId(), cloned_data->GetId());
}

TEST_F(BrowserFrameContextDataTest, GetLocalParentOrOpener) {
  auto data = std::make_unique<BrowserFrameContextData>(
      GetRenderFrameHost(opener_web_contents()));
  {
    std::unique_ptr<ContextData> local_parent_or_opener =
        data->GetLocalParentOrOpener();

    // GetLocalParentOrOpener() should return a nullptr because the WebContents
    // doesn't have a parent or opener.
    EXPECT_FALSE(local_parent_or_opener);
  }

  auto child_data = std::make_unique<BrowserFrameContextData>(
      GetRenderFrameHost(web_contents()));
  {
    std::unique_ptr<ContextData> local_parent_or_opener =
        child_data->GetLocalParentOrOpener();

    // GetLocalParentOrOpener() should not return a nullptr because the
    // WebContents does have an opener set.
    EXPECT_TRUE(local_parent_or_opener);
  }
}

TEST_F(BrowserFrameContextDataTest, UrlAndOriginGetters) {
  {
    auto data = std::make_unique<BrowserFrameContextData>(
        GetRenderFrameHost(opener_web_contents()));

    EXPECT_EQ(data->GetUrl(), google_url_);
    EXPECT_EQ(data->GetOrigin(), url::Origin::Create(google_url_));
  }

  // Create a WebContents without navigating it to test the default return
  // values of GetUrl() and GetOrigin() when the URL is empty.
  {
    auto site_instance = content::SiteInstance::Create(browser_context());
    auto web_contents = content::WebContentsTester::CreateTestWebContents(
        browser_context(), site_instance);
    auto data = std::make_unique<BrowserFrameContextData>(
        GetRenderFrameHost(web_contents.get()));
    EXPECT_EQ(data->GetUrl(), GURL(url::kAboutBlankURL));
    EXPECT_EQ(data->GetOrigin().GetURL(), GURL());
  }
}

}  // namespace extensions
