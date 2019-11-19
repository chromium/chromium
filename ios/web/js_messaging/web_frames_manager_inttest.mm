// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/test/web_int_test.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// WebFramesManagerTest is parameterized on this enum to test both
// LegacyNavigationManager and WKBasedNavigationManager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// URL of a page with test links.
const char kLinksPageURL[] = "/links.html";

// Text of the link to |kPonyPageURL| on |kLinksPageURL|.
const char kLinksPagePonyLinkText[] = "Normal Link";

// ID of the link to |kPonyPageURL| on |kLinksPageURL|.
const char kLinksPagePonyLinkID[] = "normal-link";

// Text of the same page link on |kLinksPageURL|.
const char kLinksPageSamePageLinkText[] = "Same-page Link";

// ID of the same page link on |kLinksPageURL|.
const char kLinksPageSamePageLinkID[] = "same-page-link";

// URL of a page linked to by a link on |kLinksPageURL|.
const char kPonyPageURL[] = "/pony.html";

// Text on |kPonyPageURL|.
const char kPonyPageText[] = "Anyone know any good pony jokes?";
}  // namespace

namespace web {

// Test fixture for integration tests involving html5 window.history state
// operations.
class WebFramesManagerTest
    : public WebIntTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  void SetUp() override {
    // Set feature flag before calling SetUp (which relies on those feature
    // flags).
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }

    WebIntTest::SetUp();

    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    net::test_server::RegisterDefaultHandlers(test_server_.get());
    test_server_->ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/"));
    ASSERT_TRUE(test_server_->Start());
  }

 protected:
  // Embedded test server which hosts sample pages.
  std::unique_ptr<net::EmbeddedTestServer> test_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the WebFramesManager correctly adds a WebFrame for a webpage.
TEST_P(WebFramesManagerTest, SingleWebFrame) {
  WebFramesManager* frames_manager = web_state()->GetWebFramesManager();

  GURL url = test_server_->GetURL("/echo");
  ASSERT_TRUE(LoadUrl(url));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  EXPECT_FALSE(main_web_frame->GetFrameId().empty());
  EXPECT_EQ(url.GetOrigin(), main_web_frame->GetSecurityOrigin());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back.
TEST_P(WebFramesManagerTest, SingleWebFrameBack) {
  WebFramesManager* frames_manager = web_state()->GetWebFramesManager();

  // Load first page.
  GURL url = test_server_->GetURL("/echo");
  ASSERT_TRUE(LoadUrl(url));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  std::string frame_id = main_web_frame->GetFrameId();
  EXPECT_FALSE(frame_id.empty());

  // Load second page.
  GURL pony_url = test_server_->GetURL(kPonyPageURL);
  ASSERT_TRUE(LoadUrl(pony_url));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  WebFrame* pony_main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(pony_main_web_frame);
  EXPECT_TRUE(pony_main_web_frame->IsMainFrame());
  EXPECT_EQ(pony_url.GetOrigin(), pony_main_web_frame->GetSecurityOrigin());

  std::string pony_frame_id = pony_main_web_frame->GetFrameId();
  EXPECT_FALSE(pony_frame_id.empty());
  EXPECT_NE(frame_id, pony_frame_id);

  // Navigate back to first page.
  navigation_manager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  EXPECT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_EQ(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back from a clicked link.
TEST_P(WebFramesManagerTest, SingleWebFrameLinkNavigationBackForward) {
  WebFramesManager* frames_manager = web_state()->GetWebFramesManager();

  // Load page with links.
  GURL url = test_server_->GetURL(kLinksPageURL);
  ASSERT_TRUE(LoadUrl(url));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  std::string frame_id = main_web_frame->GetFrameId();

  // Navigate to a linked page.
  GURL pony_url = test_server_->GetURL(kPonyPageURL);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(pony_url, ^{
    ASSERT_TRUE(
        web::test::TapWebViewElementWithId(web_state(), kLinksPagePonyLinkID));
  }));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  WebFrame* pony_main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(pony_main_web_frame);
  EXPECT_TRUE(pony_main_web_frame->IsMainFrame());
  EXPECT_EQ(pony_url.GetOrigin(), pony_main_web_frame->GetSecurityOrigin());

  std::string pony_frame_id = pony_main_web_frame->GetFrameId();
  EXPECT_FALSE(pony_frame_id.empty());
  EXPECT_NE(frame_id, pony_frame_id);

  // Go back to the links page.
  navigation_manager()->GoBack();
  ASSERT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kLinksPagePonyLinkText));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_EQ(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());

  // Go forward to the linked page.
  navigation_manager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kPonyPageText));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_NE(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back from a clicked same page link.
TEST_P(WebFramesManagerTest, SingleWebFrameSamePageNavigationBackForward) {
  WebFramesManager* frames_manager = web_state()->GetWebFramesManager();

  GURL url = test_server_->GetURL(kLinksPageURL);
  ASSERT_TRUE(LoadUrl(url));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  std::string frame_id = main_web_frame->GetFrameId();

  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                 kLinksPageSamePageLinkID));

  // WebFrame should not have changed.
  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());

  navigation_manager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 kLinksPageSamePageLinkText));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());

  navigation_manager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 kLinksPageSamePageLinkText));

  ASSERT_EQ(1ul, frames_manager->GetAllWebFrames().size());
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticWebFramesManagerTest,
                         WebFramesManagerTest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));

}  // namespace web
