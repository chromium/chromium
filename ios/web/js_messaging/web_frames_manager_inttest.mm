// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/test/web_int_test.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gtest/include/gtest/gtest.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// URL of a page with test links.
const char kLinksPageURL[] = "/links.html";

// Text of the link to `kPonyPageURL` on `kLinksPageURL`.
const char kLinksPagePonyLinkText[] = "Normal Link";

// ID of the link to `kPonyPageURL` on `kLinksPageURL`.
const char kLinksPagePonyLinkID[] = "normal-link";

// Text of the same page link on `kLinksPageURL`.
const char kLinksPageSamePageLinkText[] = "Same-page Link";

// ID of the same page link on `kLinksPageURL`.
const char kLinksPageSamePageLinkID[] = "same-page-link";

// URL of a page linked to by a link on `kLinksPageURL`.
const char kPonyPageURL[] = "/pony.html";

// Text on `kPonyPageURL`.
const char kPonyPageText[] = "Anyone know any good pony jokes?";
}  // namespace

namespace web {

// Test fixture for integration tests involving html5 window.history state
// operations.
class WebFramesManagerTest : public WebIntTest {
 protected:
  void SetUp() override {
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
};

// Tests that the WebFramesManager correctly adds a WebFrame for a webpage.
TEST_F(WebFramesManagerTest, SingleWebFrame) {
  WebFramesManager* frames_manager =
      web_state()->GetPageWorldWebFramesManager();

  GURL url = test_server_->GetURL("/echo");
  ASSERT_TRUE(LoadUrl(url));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  EXPECT_FALSE(main_web_frame->GetFrameId().empty());
  EXPECT_EQ(url.DeprecatedGetOriginAsURL(),
            main_web_frame->GetSecurityOrigin());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back.
TEST_F(WebFramesManagerTest, SingleWebFrameBack) {
  WebFramesManager* frames_manager =
      web_state()->GetPageWorldWebFramesManager();

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

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  WebFrame* pony_main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(pony_main_web_frame);
  EXPECT_TRUE(pony_main_web_frame->IsMainFrame());
  EXPECT_EQ(pony_url.DeprecatedGetOriginAsURL(),
            pony_main_web_frame->GetSecurityOrigin());

  std::string pony_frame_id = pony_main_web_frame->GetFrameId();
  EXPECT_FALSE(pony_frame_id.empty());
  EXPECT_NE(frame_id, pony_frame_id);

  // Navigate back to first page.
  navigation_manager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_EQ(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back from a clicked link.
TEST_F(WebFramesManagerTest, SingleWebFrameLinkNavigationBackForward) {
  WebFramesManager* frames_manager =
      web_state()->GetPageWorldWebFramesManager();

  // Load page with links.
  GURL url = test_server_->GetURL(kLinksPageURL);
  ASSERT_TRUE(LoadUrl(url));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  std::string frame_id = main_web_frame->GetFrameId();

  // Navigate to a linked page.
  GURL pony_url = test_server_->GetURL(kPonyPageURL);
  auto block = ^{
    ASSERT_TRUE(
        web::test::TapWebViewElementWithId(web_state(), kLinksPagePonyLinkID));
  };
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(pony_url, block));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  WebFrame* pony_main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(pony_main_web_frame);
  EXPECT_TRUE(pony_main_web_frame->IsMainFrame());
  EXPECT_EQ(pony_url.DeprecatedGetOriginAsURL(),
            pony_main_web_frame->GetSecurityOrigin());

  std::string pony_frame_id = pony_main_web_frame->GetFrameId();
  EXPECT_FALSE(pony_frame_id.empty());
  EXPECT_NE(frame_id, pony_frame_id);

  // Go back to the links page.
  navigation_manager()->GoBack();
  ASSERT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kLinksPagePonyLinkText));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_EQ(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());

  // Go forward to the linked page.
  navigation_manager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), kPonyPageText));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_NE(frame_id, frames_manager->GetMainWebFrame()->GetFrameId());
}

// Tests that the WebFramesManager correctly adds a unique WebFrame after a
// webpage navigates back from a clicked same page link.
TEST_F(WebFramesManagerTest, SingleWebFrameSamePageNavigationBackForward) {
  WebFramesManager* frames_manager =
      web_state()->GetPageWorldWebFramesManager();

  GURL url = test_server_->GetURL(kLinksPageURL);
  ASSERT_TRUE(LoadUrl(url));

  WebFrame* main_web_frame = frames_manager->GetMainWebFrame();
  ASSERT_TRUE(main_web_frame);
  EXPECT_TRUE(main_web_frame->IsMainFrame());
  std::string frame_id = main_web_frame->GetFrameId();

  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(),
                                                 kLinksPageSamePageLinkID));

  // WebFrame should not have changed.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());

  navigation_manager()->GoBack();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 kLinksPageSamePageLinkText));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());

  navigation_manager()->GoForward();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(),
                                                 kLinksPageSamePageLinkText));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return frames_manager->GetAllWebFrames().size() == 1;
  }));
  EXPECT_FALSE(frames_manager->GetMainWebFrame()->GetFrameId().empty());
  EXPECT_EQ(main_web_frame, frames_manager->GetMainWebFrame());
}

}  // namespace web
