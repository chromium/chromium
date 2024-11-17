// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "components/sessions/core/session_id.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/gfx/image/image_unittest_util.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// A text string that is included in `kTestPageHTML`.
const char kTextInTestPageHTML[] = "this_is_a_test_string";

// A test page HTML containing `kTextInTestPageHTML`.
const char kTestPageHTML[] = "<html><body>this_is_a_test_string</body><html>";

// Returns the NSURLErrorUnsupportedURL error expected for tests.
NSError* CreateUnsupportedURLError() {
  return web::testing::CreateErrorWithUnderlyingErrorChain(
      {{NSURLErrorDomain, NSURLErrorUnsupportedURL},
       {net::kNSErrorDomain, net::ERR_INVALID_URL}});
}

// Create an unrealized WebState with `items_count` navigation items.
std::unique_ptr<WebState> CreateUnrealizedWebStateWithItemsCount(
    BrowserState* browser_state,
    size_t items_count) {
  std::vector<test::PageInfo> items;
  items.reserve(items_count);

  for (size_t index = 0; index < items_count; ++index) {
    items.push_back(test::PageInfo{
        .url = GURL(base::StringPrintf("http://www.%zu.com", index)),
        .title = base::StringPrintf("Test%zu", index),
    });
  }

  return test::CreateUnrealizedWebStateWithItems(
      browser_state, /* last_committed_item_index= */ 0, items);
}

}  // namespace

// Test fixture for web::WebTest class.
class WebStateTest : public FakeWebClient, public WebTestWithWebState {
  void SetUp() override {
    WebTestWithWebState::SetUp();
    web::IgnoreOverRealizationCheck();
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that executing user JavaScript registers user interaction.
TEST_F(WebStateTest, UserScriptExecution) {
  web::FakeWebStateDelegate delegate;
  web_state()->SetDelegate(&delegate);
  ASSERT_TRUE(delegate.child_windows().empty());

  ASSERT_TRUE(LoadHtml("<html></html>"));
  web_state()->ExecuteUserJavaScript(@"window.open('', target='_blank');");

  web::FakeWebStateDelegate* delegate_ptr = &delegate;
  bool suceess = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    // Child window can only be open if the user interaction was registered.
    return delegate_ptr->child_windows().size() == 1;
  });

  ASSERT_TRUE(suceess);
  EXPECT_TRUE(delegate.child_windows()[0]);
}

// Tests loading progress.
TEST_F(WebStateTest, LoadingProgress) {
  EXPECT_FLOAT_EQ(0.0, web_state()->GetLoadingProgress());
  ASSERT_TRUE(LoadHtml("<html></html>"));
  EXPECT_TRUE(WaitForCondition(^bool() {
    return web_state()->GetLoadingProgress() == 1.0;
  }));
}

// Tests that reload with web::ReloadType::NORMAL is no-op when navigation
// manager is empty.
TEST_F(WebStateTest, ReloadWithNormalTypeWithEmptyNavigationManager) {
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());

  navigation_manager->Reload(web::ReloadType::NORMAL,
                             false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());
}

// Tests that reload with web::ReloadType::ORIGINAL_REQUEST_URL is no-op when
// navigation manager is empty.
TEST_F(WebStateTest, ReloadWithOriginalTypeWithEmptyNavigationManager) {
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());

  navigation_manager->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                             false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());
}

// Tests that the snapshot method returns an image of a rendered html page.
TEST_F(WebStateTest, Snapshot) {
  ASSERT_TRUE(
      LoadHtml("<html><div style='background-color:#FF0000; width:50%; "
               "height:100%;'></div></html>"));
  __block bool snapshot_complete = false;
  [GetAnyKeyWindow() addSubview:web_state()->GetView()];
  // The subview is added but not immediately painted, so a small delay is
  // necessary.
  CGRect rect = [web_state()->GetView() bounds];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.2));
  web_state()->TakeSnapshot(
      rect, base::BindRepeating(^(UIImage* snapshot) {
        ASSERT_FALSE(!snapshot);
        EXPECT_GT(snapshot.size.width, 0);
        EXPECT_GT(snapshot.size.height, 0);
        int red_pixel_x = (snapshot.size.width / 2) - 10;
        int white_pixel_x = (snapshot.size.width / 2) + 10;
        // Test a pixel on the left (red) side.
        gfx::test::CheckColors(
            gfx::test::GetPlatformImageColor(snapshot, red_pixel_x, 50),
            SK_ColorRED);
        // Test a pixel on the right (white) side.
        gfx::test::CheckColors(
            gfx::test::GetPlatformImageColor(snapshot, white_pixel_x, 50),
            SK_ColorWHITE);
        snapshot_complete = true;
      }));
  EXPECT_TRUE(WaitForCondition(^{
    return snapshot_complete;
  }));
}

// Tests that the create PDF method returns a PDF of a rendered html page.
TEST_F(WebStateTest, CreateFullPagePdf_ValidURL) {
  [GetAnyKeyWindow() addSubview:web_state()->GetView()];

  // Load a URL and some HTML in the WebState.
  GURL url("https://www.chromium.org");
  NavigationManager::WebLoadParams load_params(url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return web_state()->GetLastCommittedURL() == url &&
           !web_state()->IsLoading();
  }));

  NSString* data_html =
      @"<html><div style='background-color:#FF0000; width:50%; "
       "height:100%;'></div></html>";
  web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", url);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return !web_state()->IsLoading();
  }));

  // Create a PDF for this page and validate the data.
  __block NSData* callback_data = nil;
  web_state()->CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
    callback_data = [pdf_document_data copy];
  }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return callback_data;
  }));

  CGPDFDocumentRef pdf = CGPDFDocumentCreateWithProvider(
      CGDataProviderCreateWithCFData((CFDataRef)callback_data));
  CGSize pdf_size =
      CGPDFPageGetBoxRect(CGPDFDocumentGetPage(pdf, 1), kCGPDFMediaBox).size;

  CGFloat kSaveAreaTopInset = GetAnyKeyWindow().safeAreaInsets.top;
  EXPECT_GE(pdf_size.height,
            UIScreen.mainScreen.bounds.size.height - kSaveAreaTopInset);
  EXPECT_GE(pdf_size.width, [[UIScreen mainScreen] bounds].size.width);

  CGPDFDocumentRelease(pdf);
}

// Tests that CreateFullPagePdf invokes completion callback nil when an invalid
// URL is loaded.
TEST_F(WebStateTest, CreateFullPagePdf_InvalidURLs) {
  GURL app_specific_url(
      base::StringPrintf("%s://app_specific_url", kTestAppSpecificScheme));

  // Empty URL and app-specific URLs (e.g. app_specific_url) should get nil
  // data through the completion callback.
  std::vector<GURL> invalid_urls = {GURL(), app_specific_url};
  NSString* data_html = @(kTestPageHTML);
  for (auto& url : invalid_urls) {
    web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                          @"text/html", url);

    NavigationManager::WebLoadParams load_params(url);
    web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForPageLoadTimeout, ^bool {
          return web_state()->GetLastCommittedURL() == url;
        }));

    __block NSData* callback_data = nil;
    __block bool callback_called = false;
    web_state()->CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
      callback_data = [pdf_document_data copy];
      callback_called = true;
    }));

    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return callback_called;
    }));

    ASSERT_FALSE(callback_data);
  }
}

// Tests that CreateFullPagePdf invokes completion callback nil when the
// WebState content is not HTML (e.g. a PDF file).
TEST_F(WebStateTest, CreateFullPagePdfWebStatePdfContent) {
  CGRect fake_bounds = CGRectMake(0, 0, 100, 100);
  UIGraphicsPDFRenderer* pdf_renderer =
      [[UIGraphicsPDFRenderer alloc] initWithBounds:fake_bounds];
  NSData* pdf_data = [pdf_renderer
      PDFDataWithActions:^(UIGraphicsPDFRendererContext* context) {
        [context beginPage];
        [[UIColor blueColor] setFill];
        [context fillRect:fake_bounds];
      }];

  GURL test_url("https://www.chromium.org/somePDF.pdf");
  NavigationManager::WebLoadParams load_params(test_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return web_state()->GetLastCommittedURL() == test_url &&
               !web_state()->IsLoading();
      }));

  std::string mime_type = "application/pdf";
  web_state()->LoadData(
      pdf_data, [NSString stringWithUTF8String:mime_type.c_str()], test_url);

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return !web_state()->IsLoading();
      }));

  __block NSData* callback_data = nil;
  __block bool callback_called = false;
  web_state()->CreateFullPagePdf(base::BindOnce(^(NSData* pdf_document_data) {
    callback_data = [pdf_document_data copy];
    callback_called = true;
  }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
    return callback_called;
  }));

  ASSERT_FALSE(callback_data);
}

// Tests that the web state has an opener after calling SetHasOpener().
TEST_F(WebStateTest, SetHasOpener) {
  ASSERT_FALSE(web_state()->HasOpener());
  web_state()->SetHasOpener(true);
  EXPECT_TRUE(web_state()->HasOpener());
}

// Verifies that large session can be restored with max session size limit
// equals to `wk_navigation_util::kMaxSessionSize`.
TEST_F(WebStateTest, RestoreLargeSession) {
  // Create session storage with large number of items.
  const int kItemCount = 150;
  std::unique_ptr<WebState> web_state =
      CreateUnrealizedWebStateWithItemsCount(GetBrowserState(), kItemCount);

  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/41407753): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  const int maxSessionSize = kItemCount;
  const ui::PageTransition transition_type = ui::PAGE_TRANSITION_FIRST;
  // Verify that session was fully restored.
  auto block = ^{
    bool restored = navigation_manager->GetItemCount() == maxSessionSize &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      EXPECT_FALSE(navigation_manager->GetLastCommittedItem());
      EXPECT_EQ(-1, navigation_manager->GetLastCommittedItemIndex());
      EXPECT_TRUE(web_state_ptr->GetLastCommittedURL().is_empty());
      EXPECT_FALSE(navigation_manager->CanGoForward());
      EXPECT_TRUE(navigation_manager->GetBackwardItems().empty());
      EXPECT_TRUE(navigation_manager->GetForwardItems().empty());
      EXPECT_EQ("Test0", base::UTF16ToASCII(web_state_ptr->GetTitle()));
      EXPECT_EQ(0.0, web_state_ptr->GetLoadingProgress());
      EXPECT_EQ(-1, navigation_manager->GetPendingItemIndex());
      EXPECT_FALSE(navigation_manager->GetPendingItem());
    } else {
      EXPECT_EQ("Test0", base::UTF16ToASCII(web_state_ptr->GetTitle()));
      NavigationItem* last_committed_item =
          navigation_manager->GetLastCommittedItem();
      // After restoration is complete GetLastCommittedItem() will return null
      // until fist post-restore navigation is finished.
      if (last_committed_item) {
        EXPECT_EQ("http://www.0.com/", last_committed_item->GetURL());
        EXPECT_EQ("http://www.0.com/", web_state_ptr->GetLastCommittedURL());
        EXPECT_EQ(0, navigation_manager->GetLastCommittedItemIndex());
        EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
            navigation_manager->GetLastCommittedItem()->GetTransitionType(),
            transition_type));
      } else {
        EXPECT_EQ("", web_state_ptr->GetLastCommittedURL());
        EXPECT_EQ(-1, navigation_manager->GetLastCommittedItemIndex());
        NavigationItem* pending_item = navigation_manager->GetPendingItem();
        EXPECT_TRUE(pending_item);
        if (pending_item) {
          EXPECT_EQ("http://www.0.com/", pending_item->GetURL());
        }
      }
      EXPECT_TRUE(navigation_manager->GetBackwardItems().empty());
      EXPECT_EQ(std::max(navigation_manager->GetItemCount() - 1, 0),
                static_cast<int>(navigation_manager->GetForwardItems().size()));
    }
    EXPECT_FALSE(web_state_ptr->IsCrashed());
    EXPECT_FALSE(web_state_ptr->IsEvicted());
    EXPECT_EQ("http://www.0.com/", web_state_ptr->GetVisibleURL());
    NavigationItem* visible_item = navigation_manager->GetVisibleItem();
    EXPECT_TRUE(visible_item);
    EXPECT_TRUE(visible_item && visible_item->GetURL() == "http://www.0.com/");
    EXPECT_FALSE(navigation_manager->CanGoBack());

    return restored;
  };
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, block));
  EXPECT_EQ(maxSessionSize, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 100, 1);

  // Now wait until the last committed item is fully loaded.
  auto block2 = ^{
    return !navigation_manager->GetPendingItem() &&
           !web_state_ptr->IsLoading() &&
           web_state_ptr->GetLoadingProgress() == 1.0;
  };
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, block2));

  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager->GetLastCommittedItem()->GetTransitionType(),
      transition_type));

  // The restoration of www.0.com ends with displaying an error page which may
  // not be complete at this point.
  // Queue some javascript to wait for every handler to complete.
  // TODO(crbug.com/40195685): Remove this workaround.
  __block BOOL called = false;
  CRWWebController* web_controller =
      WebStateImpl::FromWebState(web_state.get())->GetWebController();
  [web_controller executeJavaScript:@"0;"
                  completionHandler:^(id, NSError*) {
                    called = true;
                  }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return called;
  }));
}

// Verifies that calling WebState::Stop() does not stop the session restoration.
// Session restoration should be opaque to the user and embedder, so calling
// Stop() is no-op.
TEST_F(WebStateTest, CallStopDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  std::unique_ptr<WebState> web_state =
      CreateUnrealizedWebStateWithItemsCount(GetBrowserState(), kItemCount);

  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/41407753): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      web_state_ptr->Stop();  // Attempt to interrupt the session restoration.
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !navigation_manager->GetPendingItem() && !web_state_ptr->IsLoading();
  }));

  // Wait for the error to be displayed.
  EXPECT_TRUE(web::test::WaitForWebViewContainingText(
      web_state_ptr, "error", base::test::ios::kWaitForJSCompletionTimeout));
}

// Verifies that calling NavigationManager::LoadURLWithParams() does not stop
// the session restoration and eventually loads the requested URL.
TEST_F(WebStateTest, CallLoadURLWithParamsDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  std::unique_ptr<WebState> web_state =
      CreateUnrealizedWebStateWithItemsCount(GetBrowserState(), kItemCount);

  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/41407753): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Attempt to interrupt the session restoration.
  GURL url("http://foo.test/");
  NavigationManager::WebLoadParams load_params(url);
  navigation_manager->LoadURLWithParams(load_params);

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      // Attempt to interrupt the session restoration multiple times, which is
      // something that the user can do on the slow network.
      navigation_manager->LoadURLWithParams(load_params);
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  // TODO(crbug.com/41477584) On Xcode 11 beta 6 this became very slow.  This
  // appears to only affect simulator, and will hopefully be fixed in a future
  // Xcode release.  Revert this to `kWaitForPageLoadTimeout` alone when fixed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout * 7, ^{
    return web_state_ptr->GetLastCommittedURL() == url;
  }));
}

// Verifies that calling NavigationManager::Reload() does not stop the session
// restoration. Session restoration should be opaque to the user and embedder,
// so calling Reload() is no-op.
TEST_F(WebStateTest, CallReloadDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  std::unique_ptr<WebState> web_state =
      CreateUnrealizedWebStateWithItemsCount(GetBrowserState(), kItemCount);

  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/41407753): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      // Attempt to interrupt the session restoration.
      navigation_manager->Reload(web::ReloadType::NORMAL,
                                 /*check_for_repost=*/false);
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !navigation_manager->GetPendingItem() && !web_state_ptr->IsLoading();
  }));

  // Wait for the error to be displayed.
  EXPECT_TRUE(web::test::WaitForWebViewContainingText(
      web_state_ptr, "error", base::test::ios::kWaitForJSCompletionTimeout));
}

// Verifies that each page title is restored.
TEST_F(WebStateTest, RestorePageTitles) {
  // Create session storage.
  const int kItemCount = 3;
  std::unique_ptr<WebState> web_state =
      CreateUnrealizedWebStateWithItemsCount(GetBrowserState(), kItemCount);

  web_state->SetKeepRenderProcessAlive(true);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/41407753): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return navigation_manager->GetItemCount() == kItemCount;
  }));

  for (unsigned int i = 0; i < kItemCount; i++) {
    NavigationItem* item = navigation_manager->GetItemAtIndex(i);
    EXPECT_EQ(GURL(base::StringPrintf("http://www.%u.com", i)),
              item->GetVirtualURL());
    EXPECT_EQ(base::ASCIIToUTF16(base::StringPrintf("Test%u", i)),
              item->GetTitle());
    EXPECT_EQ(base::ASCIIToUTF16(base::StringPrintf("Test%u", i)),
              item->GetTitleForDisplay());
  }
}

// Tests that loading an HTML page after a failed navigation works.
TEST_F(WebStateTest, LoadChromeThenHTML) {
  GURL app_specific_url(
      base::StringPrintf("%s://app_specific_url", kTestAppSpecificScheme));
  web::NavigationManager::WebLoadParams load_params(app_specific_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  // Wait for the error loading and check that it corresponds with
  // kUnsupportedUrlErrorPage.
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), app_specific_url,
                                         CreateUnsupportedURLError(),
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*cert_status=*/0)));
  NSString* data_html = @(kTestPageHTML);
  web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", GURL("https://www.chromium.org"));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kTextInTestPageHTML));
}

// Tests that loading an arbitrary file URL is a no-op.
TEST_F(WebStateTest, LoadFileURL) {
  GURL file_url("file:///path/to/file.html");
  web::NavigationManager::WebLoadParams load_params(file_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  EXPECT_FALSE(web_state()->IsLoading());
}

// Tests that reloading after loading HTML page will load the online page.
TEST_F(WebStateTest, LoadChromeThenWaitThenHTMLThenReload) {
  net::EmbeddedTestServer server;
  net::test_server::RegisterDefaultHandlers(&server);
  ASSERT_TRUE(server.Start());
  GURL echo_url = server.GetURL("/echo");

  GURL app_specific_url(
      base::StringPrintf("%s://app_specific_url", kTestAppSpecificScheme));
  web::NavigationManager::WebLoadParams load_params(app_specific_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  // Wait for the error loading.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), testing::GetErrorText(web_state(), app_specific_url,
                                         CreateUnsupportedURLError(),
                                         /*is_post=*/false, /*is_otr=*/false,
                                         /*cert_status=*/0)));
  NSString* data_html = @(kTestPageHTML);
  web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", echo_url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kTextInTestPageHTML));

  web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL, true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
  web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL, true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
}

}  // namespace web
