// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#import <memory>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_timeouts.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/features.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#import "ios/web/navigation/block_universal_links_buildflags.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/test/fakes/crw_fake_web_view_content_view.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state_observer.h"
#import "ios/web/public/test/fakes/fake_web_state_policy_decider.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/crw_fake_wk_frame_info.h"
#import "ios/web/test/fakes/crw_fake_wk_navigation_action.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/url_conversions.h"
#import "net/cert/x509_util_apple.h"
#import "net/ssl/ssl_info.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "url/scheme_host_port.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

// Subclass of WKWebView to check that the observers are removed when the web
// state is destroyed.
@interface CRWFakeWKWebViewObserverCount : WKWebView

// Array storing the different key paths observed.
@property(nonatomic, strong) NSMutableArray<NSString*>* keyPaths;
// Whether there was observers when the WebView was stopped.
@property(nonatomic, assign) BOOL hadObserversWhenStopping;

@end

@implementation CRWFakeWKWebViewObserverCount

- (void)stopLoading {
  [super stopLoading];
  self.hadObserversWhenStopping =
      self.hadObserversWhenStopping || self.keyPaths.count > 0;
}

- (void)removeObserver:(NSObject*)observer forKeyPath:(NSString*)keyPath {
  [super removeObserver:observer forKeyPath:keyPath];
  [self.keyPaths removeObject:keyPath];
}

- (void)addObserver:(NSObject*)observer
         forKeyPath:(NSString*)keyPath
            options:(NSKeyValueObservingOptions)options
            context:(void*)context {
  [super addObserver:observer
          forKeyPath:keyPath
             options:options
             context:context];
  if (!self.keyPaths) {
    self.keyPaths = [[NSMutableArray alloc] init];
  }
  [self.keyPaths addObject:keyPath];
}

@end

namespace web {
namespace {

// Syntactically invalid URL per rfc3986.
const char kInvalidURL[] = "http://%3";

const char kTestDataURL[] = "data:text/html,";

const char kTestURLString[] = "http://www.google.com/";
const char kTestAppSpecificURL[] = "testwebui://test/";

const char kTestMimeType[] = "application/vnd.test";

// Returns HTML for an optionally zoomable test page with `zoom_state`.
enum PageScalabilityType {
  PAGE_SCALABILITY_DISABLED = 0,
  PAGE_SCALABILITY_ENABLED,
};

}  // namespace

// Test fixture for testing CRWWebController. Stubs out web view.
class CRWWebControllerTest : public WebTestWithWebController {
 protected:
  CRWWebControllerTest()
      : WebTestWithWebController(std::make_unique<FakeWebClient>()) {}

  void SetUp() override {
    WebTestWithWebController::SetUp();

    fake_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    mock_web_view_ = CreateMockWebView(fake_wk_list_);
    scroll_view_ = [[UIScrollView alloc] init];
    SetWebViewURL(@(kTestURLString));
    [[[mock_web_view_ stub] andReturn:scroll_view_] scrollView];

    CRWFakeWebViewContentView* web_view_content_view =
        [[CRWFakeWebViewContentView alloc] initWithMockWebView:mock_web_view_
                                                    scrollView:scroll_view_];
    [web_controller() injectWebViewContentView:web_view_content_view];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_web_view_);
    [web_controller() resetInjectedWebViewContentView];
    WebTestWithWebController::TearDown();
  }

  FakeWebClient* GetWebClient() override {
    return static_cast<FakeWebClient*>(
        WebTestWithWebController::GetWebClient());
  }

  // The value for web view OCMock objects to expect for `-setFrame:`.
  CGRect GetExpectedWebViewFrame() const {
    CGSize container_view_size = GetAnyKeyWindow().bounds.size;

    container_view_size.height -= CGRectGetHeight(
        GetAnyKeyWindow().windowScene.statusBarManager.statusBarFrame);
    return {CGPointZero, container_view_size};
  }

  void SetWebViewURL(NSString* url_string) {
    test_url_ = [NSURL URLWithString:url_string];
  }

  // Creates WebView mock.
  UIView* CreateMockWebView(CRWFakeBackForwardList* wk_list) {
    WKWebView* result = [OCMockObject mockForClass:[WKWebView class]];

    OCMStub([result backForwardList]).andReturn(wk_list);
    // This uses `andDo` rather than `andReturn` since the URL it returns needs
    // to change when `test_url_` changes.
    OCMStub([result URL]).andDo(^(NSInvocation* invocation) {
      [invocation setReturnValue:&test_url_];
    });
    OCMStub(
        [result setNavigationDelegate:[OCMArg checkWithBlock:^(id delegate) {
                  navigation_delegate_ = delegate;
                  return YES;
                }]]);
    OCMStub([result serverTrust]);
    OCMStub([result setUIDelegate:OCMOCK_ANY]);
    OCMStub([result frame]).andReturn(UIScreen.mainScreen.bounds);
    OCMStub([result setCustomUserAgent:OCMOCK_ANY]);
    OCMStub([result customUserAgent]);
    OCMStub([static_cast<WKWebView*>(result) loadRequest:OCMOCK_ANY]);
    OCMStub([static_cast<WKWebView*>(result) loadFileURL:OCMOCK_ANY
                                 allowingReadAccessToURL:OCMOCK_ANY]);
    OCMStub([result setFrame:GetExpectedWebViewFrame()]);
    OCMStub([result addObserver:OCMOCK_ANY
                     forKeyPath:OCMOCK_ANY
                        options:0
                        context:nullptr]);
    OCMStub([result removeObserver:OCMOCK_ANY forKeyPath:OCMOCK_ANY]);
    OCMStub([result evaluateJavaScript:OCMOCK_ANY
                     completionHandler:OCMOCK_ANY]);
    OCMStub([result evaluateJavaScript:OCMOCK_ANY
                               inFrame:OCMOCK_ANY
                        inContentWorld:OCMOCK_ANY
                     completionHandler:OCMOCK_ANY]);
    OCMStub([result allowsBackForwardNavigationGestures]);
    OCMStub([result setAllowsBackForwardNavigationGestures:NO]);
    OCMStub([result setAllowsBackForwardNavigationGestures:YES]);
    OCMStub([result isLoading]);
    OCMStub([result stopLoading]);
    OCMStub([result removeFromSuperview]);
    OCMStub([result hasOnlySecureContent]).andReturn(YES);
    OCMStub([(WKWebView*)result title]).andReturn(@"Title");

    return result;
  }

  __weak id<WKNavigationDelegate> navigation_delegate_;
  UIScrollView* scroll_view_;
  id mock_web_view_;
  CRWFakeBackForwardList* fake_wk_list_;
  NSURL* test_url_;
};

// Tests that when a committed but not-yet-finished navigation is cancelled,
// the navigation item's ErrorRetryStateMachine is updated correctly.
TEST_F(CRWWebControllerTest, CancelCommittedNavigation) {
  [[[mock_web_view_ stub] andReturnBool:NO] hasOnlySecureContent];
  [static_cast<WKWebView*>([[mock_web_view_ stub] andReturn:@""]) title];

  WKNavigation* navigation =
      static_cast<WKNavigation*>([[NSObject alloc] init]);
  SetWebViewURL(@"http://chromium.test");
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:navigation];
  [fake_wk_list_ setCurrentURL:@"http://chromium.test"];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:navigation];
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorCancelled
                                   userInfo:nil];
  [navigation_delegate_ webView:mock_web_view_
              didFailNavigation:navigation
                      withError:error];
}

// Tests returning pending item stored in navigation context.
TEST_F(CRWWebControllerTest, TestPendingItem) {
  ASSERT_FALSE([web_controller() lastPendingItemForNewNavigation]);

  // Create pending item by simulating a renderer-initiated navigation.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];

  NavigationItemImpl* item = [web_controller() lastPendingItemForNewNavigation];

  ASSERT_TRUE(item);
  EXPECT_EQ(kTestURLString, item->GetURL());
}

// Tests allowsBackForwardNavigationGestures default value and negating this
// property.
TEST_F(CRWWebControllerTest, SetAllowsBackForwardNavigationGestures) {
  EXPECT_TRUE(web_controller().allowsBackForwardNavigationGestures);
  web_controller().allowsBackForwardNavigationGestures = NO;
  EXPECT_FALSE(web_controller().allowsBackForwardNavigationGestures);
}

// Tests that a web view is created after calling -[ensureWebViewCreated] and
// check its user agent.
TEST_F(CRWWebControllerTest, WebViewCreatedAfterEnsureWebViewCreated) {
  FakeWebClient* web_client = static_cast<FakeWebClient*>(GetWebClient());

  [web_controller() removeWebView];
  WKWebView* web_view = [web_controller() ensureWebViewCreated];
  EXPECT_TRUE(web_view);
  EXPECT_NSEQ(
      base::SysUTF8ToNSString(web_client->GetUserAgent(UserAgentType::MOBILE)),
      web_view.customUserAgent);

  web_client->SetDefaultUserAgent(UserAgentType::DESKTOP);
  [web_controller() removeWebView];
  web_view = [web_controller() ensureWebViewCreated];
  EXPECT_NSEQ(
      base::SysUTF8ToNSString(web_client->GetUserAgent(UserAgentType::DESKTOP)),
      web_view.customUserAgent);
}

// Tests that the WebView is correctly removed/added from the view hierarchy.
TEST_F(CRWWebControllerTest, RemoveWebViewFromViewHierarchy) {
  // Make sure that the WebController view has a window to avoid stashing the
  // WebView once created.
  [GetAnyKeyWindow() addSubview:web_controller().view];

  // Get the web view.
  [web_controller() removeWebView];
  WKWebView* web_view = [web_controller() ensureWebViewCreated];

  ASSERT_EQ(web_controller().view, web_view.superview.superview);

  [web_controller() removeWebViewFromViewHierarchyForShutdown:NO];
  EXPECT_EQ(nil, web_view.superview.superview);

  [web_controller() addWebViewToViewHierarchy];
  EXPECT_EQ(web_controller().view, web_view.superview.superview);
}

// Test fixture to test JavaScriptDialogPresenter.
class JavaScriptDialogPresenterTest : public WebTestWithWebController {
 protected:
  JavaScriptDialogPresenterTest() : page_url_("https://chromium.test/") {}
  void SetUp() override {
    WebTestWithWebState::SetUp();
    LoadHtml(@"<html><body></body></html>", page_url_);
    web_state()->SetDelegate(&web_state_delegate_);
  }
  void TearDown() override {
    web_state()->SetDelegate(nullptr);
    WebTestWithWebState::TearDown();
  }
  FakeJavaScriptDialogPresenter* js_dialog_presenter() {
    return web_state_delegate_.GetFakeJavaScriptDialogPresenter();
  }
  const std::vector<std::unique_ptr<FakeJavaScriptAlertDialog>>&
  requested_alert_dialogs() {
    return js_dialog_presenter()->requested_alert_dialogs();
  }
  const std::vector<std::unique_ptr<FakeJavaScriptConfirmDialog>>&
  requested_confirm_dialogs() {
    return js_dialog_presenter()->requested_confirm_dialogs();
  }
  const std::vector<std::unique_ptr<FakeJavaScriptPromptDialog>>&
  requested_prompt_dialogs() {
    return js_dialog_presenter()->requested_prompt_dialogs();
  }
  bool JSDialogPresenterHasDialogs() {
    return !requested_alert_dialogs().empty() ||
           !requested_confirm_dialogs().empty() ||
           !requested_prompt_dialogs().empty();
  }
  const GURL& page_url() { return page_url_; }

 private:
  FakeWebStateDelegate web_state_delegate_;
  GURL page_url_;
};

// Tests that window.alert dialog is shown.
TEST_F(JavaScriptDialogPresenterTest, Alert) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  ExecuteJavaScript(@"alert('test')");

  ASSERT_EQ(1U, requested_alert_dialogs().size());
  ASSERT_TRUE(requested_confirm_dialogs().empty());
  ASSERT_TRUE(requested_prompt_dialogs().empty());
  auto& dialog = requested_alert_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_NSEQ(@"test", dialog->message_text);
}

// Tests that window.confirm dialog is shown and its result is true.
TEST_F(JavaScriptDialogPresenterTest, ConfirmWithTrue) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  js_dialog_presenter()->set_callback_success_argument(true);

  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_TRUE(requested_alert_dialogs().empty());
  ASSERT_EQ(1U, requested_confirm_dialogs().size());
  ASSERT_TRUE(requested_prompt_dialogs().empty());
  auto& dialog = requested_confirm_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_NSEQ(@"test", dialog->message_text);
}

// Tests that window.confirm dialog is shown and its result is false.
TEST_F(JavaScriptDialogPresenterTest, ConfirmWithFalse) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_TRUE(requested_alert_dialogs().empty());
  ASSERT_EQ(1U, requested_confirm_dialogs().size());
  ASSERT_TRUE(requested_prompt_dialogs().empty());
  auto& dialog = requested_confirm_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_NSEQ(@"test", dialog->message_text);
}

// Tests that window.prompt dialog is shown.
TEST_F(JavaScriptDialogPresenterTest, Prompt) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  js_dialog_presenter()->set_callback_user_input_argument(@"Maybe");

  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_TRUE(requested_alert_dialogs().empty());
  ASSERT_TRUE(requested_confirm_dialogs().empty());
  ASSERT_EQ(1U, requested_prompt_dialogs().size());
  auto& dialog = requested_prompt_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_NSEQ(@"Yes?", dialog->message_text);
  EXPECT_NSEQ(@"No", dialog->default_prompt_text);
}

// Tests that window.prompt dialog is shown even when the given message and
// default value are empty.
TEST_F(JavaScriptDialogPresenterTest, PromptEmpty) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  js_dialog_presenter()->set_callback_user_input_argument(@"Maybe");

  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('', '')"));

  ASSERT_TRUE(requested_alert_dialogs().empty());
  ASSERT_TRUE(requested_confirm_dialogs().empty());
  ASSERT_EQ(1U, requested_prompt_dialogs().size());
  auto& dialog = requested_prompt_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_NSEQ(@"", dialog->message_text);
  EXPECT_NSEQ(@"", dialog->default_prompt_text);
}

// Tests that window.alert, window.confirm and window.prompt dialogs are not
// shown if URL of presenting main frame is different from visible URL.
TEST_F(JavaScriptDialogPresenterTest, DifferentVisibleUrl) {
  ASSERT_FALSE(JSDialogPresenterHasDialogs());

  // Change visible URL.
  AddPendingItem(GURL("https://pending.test/"), ui::PAGE_TRANSITION_TYPED);
  web_controller().webStateImpl->SetIsLoading(true);
  ASSERT_NE(page_url().DeprecatedGetOriginAsURL(),
            web_state()->GetVisibleURL().DeprecatedGetOriginAsURL());

  ExecuteJavaScript(@"alert('test')");
  ASSERT_TRUE(requested_alert_dialogs().empty());

  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));
  ASSERT_TRUE(requested_confirm_dialogs().empty());

  EXPECT_NSEQ([NSNull null], ExecuteJavaScript(@"prompt('Yes?', 'No')"));
  ASSERT_TRUE(requested_prompt_dialogs().empty());
}

// Test fixture for testing visible security state.
typedef WebTestWithWebState CRWWebStateSecurityStateTest;

// Tests that loading HTTP page updates the SSLStatus.
TEST_F(CRWWebStateSecurityStateTest, LoadHttpPage) {
  FakeWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  NavigationManager* nav_manager = web_state()->GetNavigationManager();
  NavigationItem* item = nav_manager->GetLastCommittedItem();
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, item->GetSSL().security_style);
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Real WKWebView is required for CRWWebControllerInvalidUrlTest.
typedef WebTestWithWebState CRWWebControllerInvalidUrlTest;

// Tests that web controller does not navigate to about:blank if iframe src
// has invalid url. Web controller loads about:blank if page navigates to
// invalid url, but should do nothing if navigation is performed in iframe. This
// test prevents crbug.com/694865 regression.
TEST_F(CRWWebControllerInvalidUrlTest, IFrameWithInvalidURL) {
  GURL url("http://chromium.test");
  ASSERT_FALSE(GURL(kInvalidURL).is_valid());
  LoadHtml([NSString stringWithFormat:@"<iframe src='%s'/>", kInvalidURL], url);
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

// Real WKWebView is required for CRWWebControllerJSExecutionTest.
typedef WebTestWithWebController CRWWebControllerJSExecutionTest;

// Tests that a script correctly evaluates to boolean.
TEST_F(CRWWebControllerJSExecutionTest, Execution) {
  LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"true"));
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"false"));
}

// Test fixture to test decidePolicyForNavigationResponse:decisionHandler:
// delegate method.
class CRWWebControllerResponseTest : public CRWWebControllerTest {
 protected:
  CRWWebControllerResponseTest() {}

  void SetUp() override {
    CRWWebControllerTest::SetUp();
    download_delegate_ =
        std::make_unique<FakeDownloadControllerDelegate>(download_controller());
  }

  // Calls webView:decidePolicyForNavigationResponse:decisionHandler: callback
  // and waits for decision handler call. Returns false if decision handler call
  // times out.
  [[nodiscard]] bool CallDecidePolicyForNavigationResponseWithResponse(
      NSURLResponse* response,
      BOOL for_main_frame,
      BOOL can_show_mime_type,
      WKNavigationResponsePolicy* out_policy) {
    id navigation_response =
        [OCMockObject mockForClass:[WKNavigationResponse class]];
    OCMStub([navigation_response response]).andReturn(response);
    OCMStub([navigation_response isForMainFrame]).andReturn(for_main_frame);
    OCMStub([navigation_response canShowMIMEType])
        .andReturn(can_show_mime_type);

    // Check whether the NavigationController has been configured to simulate
    // a POST request (which is required to recreate a correct NSURLRequest
    // object when mocking the new download API on iOS 15+).
    NavigationItemImpl* pending_item =
        web_controller()
            .webStateImpl->GetNavigationManagerImpl()
            .GetPendingItemImpl();
    const bool has_post_data =
        pending_item && pending_item->GetPostData() != nil;

    // Call decidePolicyForNavigationResponse and wait for decisionHandler's
    // callback.
    __block bool callback_called = false;
    if (for_main_frame) {
      // webView:didStartProvisionalNavigation: is not called for iframes.
      [navigation_delegate_ webView:mock_web_view_
          didStartProvisionalNavigation:nil];
    }
    [navigation_delegate_ webView:mock_web_view_
        decidePolicyForNavigationResponse:navigation_response
                          decisionHandler:^(WKNavigationResponsePolicy policy) {
                            *out_policy = policy;
                            callback_called = true;
                          }];
    if (!WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
          return callback_called;
        })) {
      return false;
    }

    // When the new download API is enabled (which can only happen on iOS 15),
    // the interaction is a bit more complex as WebKit will call additional
    // methods on the WKNavigationDelegate before the DownloadTask is created.
    // Mock those necessary interactions.
    if (*out_policy == WKNavigationResponsePolicyDownload) {
      id mock_download = [OCMockObject mockForClass:[WKDownload class]];

      __block bool delegate_set = false;
      __block id download_delegate = nil;
      OCMStub([mock_download setDelegate:[OCMArg any]])
          .andDo(^(NSInvocation* invocation) {
            // Using __unsafe_unretained is required to extract the parameter
            // from the NSInvocation otherwise ARC will over-release.
            __weak id argument = nil;
            [invocation getArgument:&argument atIndex:2];
            download_delegate = argument;
            delegate_set = true;
          });

      [navigation_delegate_ webView:mock_web_view_
                 navigationResponse:navigation_response
                  didBecomeDownload:mock_download];

      if (!WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
            return delegate_set;
          })) {
        return false;
      }

      NSMutableURLRequest* request =
          [[NSURLRequest requestWithURL:response.URL] mutableCopy];
      if (has_post_data) {
        request.HTTPMethod = @"POST";
      }
      OCMStub([mock_download originalRequest]).andReturn(request);
      OCMStub([mock_download cancel:[OCMArg any]])
          .andDo(^(NSInvocation* invocation) {
            // Using __unsafe_unretained is required to extract the parameter
            // from the NSInvocation otherwise ARC will over-release.
            __weak void (^block)(NSData* data);
            [invocation getArgument:&block atIndex:2];
            block(nil);
          });

      [download_delegate download:mock_download
          decideDestinationUsingResponse:response
                       suggestedFilename:@"filename.txt"
                       completionHandler:^(NSURL* destination){
                       }];
    }

    return true;
  }

  // The expectation varies depending on whether the new download API is used
  // or not (as the new download API requires CRWWKNavigationHandler to return
  // a different policy). This method returns the expected policy for the test.
  [[nodiscard]] static WKNavigationResponsePolicy ExpectedPolicyForDownload() {
    return WKNavigationResponsePolicyDownload;
  }

  DownloadController* download_controller() {
    return DownloadController::FromBrowserState(GetBrowserState());
  }

  std::unique_ptr<FakeDownloadControllerDelegate> download_delegate_;
};

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// renderer-initiated navigations in main frame for http: URLs.
TEST_F(CRWWebControllerResponseTest, AllowRendererInitiatedResponse) {
  // Simulate regular navigation response with text/html MIME type.
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_->alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// renderer-initiated navigations in iframe for data: URLs.
TEST_F(CRWWebControllerResponseTest,
       AllowRendererInitiatedDataUrlResponseInIFrame) {
  // Simulate data:// url response with text/html MIME type.
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/NO, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_->alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: blocks
// rendering data URLs for renderer-initiated navigations in main frame to
// prevent abusive behavior (crbug.com/890558) and presents the download option.
TEST_F(CRWWebControllerResponseTest,
       DownloadRendererInitiatedDataUrlResponseInMainFrame) {
  // Simulate data:// url response with text/html MIME type.
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created (see crbug.com/949114).
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_EQ(kTestDataURL, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_TRUE(task->GetContentDisposition().empty());
  EXPECT_TRUE(task->GetMimeType().empty());
  EXPECT_NSEQ(@"GET", task->GetHttpMethod());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// rendering data URLs for browser-initiated navigations in main frame.
TEST_F(CRWWebControllerResponseTest,
       AllowBrowserInitiatedDataUrlResponseInMainFrame) {
  // Simulate data:// url response with text/html MIME type.
  GURL url(kTestDataURL);
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);
  [web_controller() loadCurrentURLWithRendererInitiatedNavigation:NO];
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_->alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler:
// correctly uses POST HTTP method for post requests.
TEST_F(CRWWebControllerResponseTest, DownloadForPostRequest) {
  // Simulate regular navigation response for post request with text/html MIME
  // type.
  GURL url(kTestURLString);
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);
  web_controller()
      .webStateImpl->GetNavigationManagerImpl()
      .GetPendingItemImpl()
      ->SetPostData([NSData data]);
  [web_controller() loadCurrentURLWithRendererInitiatedNavigation:NO];
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created with POST method (crbug.com/.
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_NSEQ(@"POST", task->GetHttpMethod());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSURLResponse.
TEST_F(CRWWebControllerResponseTest, DownloadWithNSURLResponse) {
  // Simulate download response.
  int64_t content_length = 10;
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kTestURLString)]
                                MIMEType:@(kTestMimeType)
                   expectedContentLength:content_length
                        textEncodingName:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(content_length, task->GetTotalBytes());
  EXPECT_EQ("", task->GetContentDisposition());
  EXPECT_EQ(kTestMimeType, task->GetMimeType());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSHTTPURLResponse.
TEST_F(CRWWebControllerResponseTest, DownloadWithNSHTTPURLResponse) {
  // Simulate download response.
  const char kContentDisposition[] = "attachment; filename=download.test";
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:@{
        @"content-disposition" : @(kContentDisposition),
      }];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ("", task->GetMimeType());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler:
// discards pending URL.
TEST_F(CRWWebControllerResponseTest, DownloadDiscardsPendingUrl) {
  GURL url(kTestURLString);
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);

  // Simulate download response.
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kTestURLString)]
                                MIMEType:@(kTestMimeType)
                   expectedContentLength:10
                        textEncodingName:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created and pending URL discarded.
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  EXPECT_EQ("", web_state()->GetVisibleURL());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSHTTPURLResponse and iframes.
TEST_F(CRWWebControllerResponseTest, IFrameDownloadWithNSHTTPURLResponse) {
  // Simulate download response.
  const char kContentDisposition[] = "attachment; filename=download.test";
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:@{
        @"content-disposition" : @(kContentDisposition),
      }];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/NO, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(ExpectedPolicyForDownload(), policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_->alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_->alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIdentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ("", task->GetMimeType());
}

// Tests `currentURL` method.
TEST_F(CRWWebControllerTest, CurrentUrl) {
  GURL url("http://chromium.test");
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);

  [[[mock_web_view_ stub] andReturnBool:NO] hasOnlySecureContent];
  [static_cast<WKWebView*>([[mock_web_view_ stub] andReturn:@""]) title];
  SetWebViewURL(@"http://chromium.test");

  // Stub out the injection process.
  [[mock_web_view_ stub] evaluateJavaScript:OCMOCK_ANY
                          completionHandler:OCMOCK_ANY];

  // Simulate a page load to trigger a URL update.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];
  [fake_wk_list_ setCurrentURL:@"http://chromium.test"];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:nil];

  EXPECT_EQ(url, [web_controller() currentURL]);
}

// Test fixture to test decidePolicyForNavigationAction:decisionHandler:
// decisionHandler's callback result.
class CRWWebControllerPolicyDeciderTest : public CRWWebControllerTest {
 protected:
  void SetUp() override {
    CRWWebControllerTest::SetUp();
  }
  // Calls webView:decidePolicyForNavigationAction:preferences:decisionHandler:
  // callback and waits for decision handler call. Returns false if decision
  // handler policy parameter didn't match `expected_policy` or if the call
  // timed out.
  [[nodiscard]] bool VerifyDecidePolicyForNavigationAction(
      NSURLRequest* request,
      WKNavigationActionPolicy expected_policy) {
    CRWFakeWKNavigationAction* navigation_action =
        [[CRWFakeWKNavigationAction alloc] init];
    navigation_action.request = request;

    CRWFakeWKFrameInfo* frame_info = [[CRWFakeWKFrameInfo alloc] init];
    frame_info.mainFrame = YES;
    navigation_action.targetFrame = frame_info;

    WKWebpagePreferences* preferences = [[WKWebpagePreferences alloc] init];

    // Call webView:decidePolicyForNavigationAction:preferences:decisionHandler:
    // and wait for decisionHandler's callback.
    __block bool policy_match = false;
    __block bool callback_called = false;
    [navigation_delegate_ webView:mock_web_view_
        decidePolicyForNavigationAction:navigation_action
                            preferences:preferences
                        decisionHandler:^(WKNavigationActionPolicy policy,
                                          WKWebpagePreferences* ignored) {
                          policy_match = expected_policy == policy;
                          callback_called = true;
                        }];
    callback_called = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      return callback_called;
    });

    return policy_match;
  }

  std::unique_ptr<BrowserState> CreateBrowserState() override {
    return std::make_unique<FakeBrowserState>();
  }

  FakeBrowserState* GetFakeBrowserState() {
    return static_cast<FakeBrowserState*>(GetBrowserState());
  }
};

// Tests that App specific URLs in iframes are allowed if the main frame is App
// specific URL. App specific pages have elevated privileges and WKWebView uses
// the same renderer process for all page frames. With that running App specific
// pages are not allowed in the same process as a web site from the internet.
TEST_F(CRWWebControllerPolicyDeciderTest,
       AllowAppSpecificIFrameFromAppSpecificPage) {
  NSURL* app_url = [NSURL URLWithString:@(kTestAppSpecificURL)];
  NSMutableURLRequest* app_url_request =
      [NSMutableURLRequest requestWithURL:app_url];
  app_url_request.mainDocumentURL = app_url;
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      app_url_request, WKNavigationActionPolicyAllow));
}

// Tests that URL is allowed in OffTheRecord mode when the
// `kBlockUniversalLinksInOffTheRecordMode` feature is disabled.
TEST_F(CRWWebControllerPolicyDeciderTest, AllowOffTheRecordNavigation) {
  GetFakeBrowserState()->SetOffTheRecord(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  url_request.mainDocumentURL = url;
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyAllow));
}

// Tests that URL is allowed in OffTheRecord mode and that universal links are
// blocked when the `kBlockUniversalLinksInOffTheRecordMode` feature is enabled
// and the BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE buildflag is set.
TEST_F(CRWWebControllerPolicyDeciderTest,
       AllowOffTheRecordNavigationBlockUniversalLinks) {
  GetFakeBrowserState()->SetOffTheRecord(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  url_request.mainDocumentURL = url;

  WKNavigationActionPolicy expected_policy = WKNavigationActionPolicyAllow;
#if BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)
  expected_policy = kNavigationActionPolicyAllowAndBlockUniversalLinks;
#endif  // BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)

  EXPECT_TRUE(
      VerifyDecidePolicyForNavigationAction(url_request, expected_policy));
}

// Tests that App specific URLs in iframes are not allowed if the main frame is
// not App specific URL.
TEST_F(CRWWebControllerPolicyDeciderTest,
       DisallowAppSpecificIFrameFromRegularPage) {
  NSURL* app_url = [NSURL URLWithString:@(kTestAppSpecificURL)];
  NSMutableURLRequest* app_url_request =
      [NSMutableURLRequest requestWithURL:app_url];
  app_url_request.mainDocumentURL = [NSURL URLWithString:@(kTestURLString)];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      app_url_request, WKNavigationActionPolicyCancel));
}

// Tests that blob URL navigation is allowed.
TEST_F(CRWWebControllerPolicyDeciderTest, BlobUrl) {
  NSURL* blob_url = [NSURL URLWithString:@"blob://aslfkh-asdkjh"];
  NSMutableURLRequest* blob_url_request =
      [NSMutableURLRequest requestWithURL:blob_url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      blob_url_request, WKNavigationActionPolicyAllow));
}

// Tests that navigations which close the WebState cancels the navigation.
// This occurs, for example, when a new page is opened with a link that is
// handled by a native application.
TEST_F(CRWWebControllerPolicyDeciderTest, ClosedWebState) {
  static CRWWebControllerPolicyDeciderTest* test_fixture = nullptr;
  test_fixture = this;
  class CloseWebStateDelegate : public FakeWebStateDelegate {
   public:
    void CloseWebState(WebState* source) override {
      test_fixture->DestroyWebState();
    }
  };
  CloseWebStateDelegate delegate;
  web_state()->SetDelegate(&delegate);

  FakeWebStatePolicyDecider policy_decider(web_state());
  policy_decider.SetShouldAllowRequest(
      web::WebStatePolicyDecider::PolicyDecision::Cancel());

  NSURL* url =
      [NSURL URLWithString:@"https://itunes.apple.com/us/album/american-radio/"
                           @"1449089454?fbclid=IwAR2NKLvDGH_YY4uZbU7Cj_"
                           @"e3h7q7DvORCI4Edvi1K9LjcUHfYObmOWl-YgE"];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

// Tests that navigations are cancelled if the web state is closed in
// `ShouldAllowRequest`.
TEST_F(CRWWebControllerPolicyDeciderTest, ClosedWebStateInShouldAllowRequest) {
  static CRWWebControllerPolicyDeciderTest* test_fixture = nullptr;
  test_fixture = this;

  class TestWebStatePolicyDecider : public WebStatePolicyDecider {
   public:
    explicit TestWebStatePolicyDecider(WebState* web_state)
        : WebStatePolicyDecider(web_state) {}
    ~TestWebStatePolicyDecider() override = default;

    // WebStatePolicyDecider overrides
    void ShouldAllowRequest(NSURLRequest* request,
                            RequestInfo request_info,
                            PolicyDecisionCallback callback) override {
      test_fixture->DestroyWebState();
      std::move(callback).Run(PolicyDecision::Allow());
    }
    void ShouldAllowResponse(NSURLResponse* response,
                             ResponseInfo response_info,
                             PolicyDecisionCallback callback) override {
      std::move(callback).Run(PolicyDecision::Allow());
    }
    void WebStateDestroyed() override {}
  };
  TestWebStatePolicyDecider policy_decider(web_state());

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

// Tests that navigations are allowed if `ShouldAllowRequest` returns a
// PolicyDecision which returns true from `ShouldAllowNavigation()`.
TEST_F(CRWWebControllerPolicyDeciderTest, AllowRequest) {
  FakeWebStatePolicyDecider policy_decider(web_state());
  policy_decider.SetShouldAllowRequest(
      web::WebStatePolicyDecider::PolicyDecision::Allow());

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyAllow));
}

// Tests that navigations are cancelled if `ShouldAllowRequest` returns a
// PolicyDecision which returns false from `ShouldAllowNavigation()`.
TEST_F(CRWWebControllerPolicyDeciderTest, CancelRequest) {
  FakeWebStatePolicyDecider policy_decider(web_state());
  policy_decider.SetShouldAllowRequest(
      web::WebStatePolicyDecider::PolicyDecision::Cancel());

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

// Tests that navigations are cancelled if `ShouldAllowRequest` returns a
// PolicyDecision which returns true from `ShouldBlockNavigation()`.
TEST_F(CRWWebControllerPolicyDeciderTest, CancelRequestAndDisplayError) {
  FakeWebStatePolicyDecider policy_decider(web_state());
  NSError* error = [NSError errorWithDomain:@"Error domain"
                                       code:123
                                   userInfo:nil];
  policy_decider.SetShouldAllowRequest(
      web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(error));

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

// Test fixture for window.open tests.
class WindowOpenByDomTest : public WebTestWithWebController {
 protected:
  WindowOpenByDomTest() : opener_url_("http://test") {}

  void SetUp() override {
    WebTestWithWebController::SetUp();
    web_state()->SetDelegate(&delegate_);
    LoadHtml(@"<html><body></body></html>", opener_url_);
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  id OpenWindowByDom() {
    NSString* const kOpenWindowScript =
        @"w = window.open('javascript:void(0);', target='_blank');"
         "w ? w.toString() : null;";
    return ExecuteJavaScript(kOpenWindowScript);
  }

  // Executes JavaScript that closes previously opened window.
  void CloseWindow() { ExecuteJavaScript(@"w.close()"); }

  // URL of a page which opens child windows.
  const GURL opener_url_;
  FakeWebStateDelegate delegate_;
};

// Tests that absence of web state delegate is handled gracefully.
TEST_F(WindowOpenByDomTest, NoDelegate) {
  web_state()->SetDelegate(nullptr);

  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(WindowOpenByDomTest, OpenWithUserGesture) {
  [web_controller() touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open executed w/o user gesture does not open a new window,
// but blocks popup instead.
TEST_F(WindowOpenByDomTest, BlockPopup) {
  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  ASSERT_EQ(1U, delegate_.popups().size());
  EXPECT_EQ(GURL("javascript:void(0);"), delegate_.popups()[0].url);
  EXPECT_EQ(opener_url_, delegate_.popups()[0].opener_url);
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_F(WindowOpenByDomTest, DontBlockPopup) {
  delegate_.allow_popups(opener_url_);
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.close closes the web state.
// TODO(crbug.com/40218609): Flaky test.
TEST_F(WindowOpenByDomTest, CloseWindow) {
  delegate_.allow_popups(opener_url_);
  ASSERT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());

  delegate_.child_windows()[0]->SetDelegate(&delegate_);
  CloseWindow();
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that calling document.write() on a newly-opened window doesn't crash.
TEST_F(WindowOpenByDomTest, DocumentWrite) {
  delegate_.allow_popups(opener_url_);

  NSString* const kDocumentWriteScript =
      @"var w = window.open();"
      @"w.document.write('<p>Hello</p>');"
      @"w.document.write(\"<meta http-equiv='refresh' content='0; url=\""
      @"+ location.toString() + \"'>\");"
      @"w.document.close();";

  ExecuteJavaScript(kDocumentWriteScript);
  EXPECT_EQ(1U, delegate_.child_windows().size());

  EXPECT_TRUE(test::WaitForWebViewNotContainingText(
      delegate_.child_windows()[0].get(), "Hello"));
}

// Tests page title changes.
typedef WebTestWithWebState CRWWebControllerTitleTest;

TEST_F(CRWWebControllerTitleTest, TitleChange) {
  // Observes and waits for TitleWasSet call.
  class TitleObserver : public WebStateObserver {
   public:
    TitleObserver() = default;

    TitleObserver(const TitleObserver&) = delete;
    TitleObserver& operator=(const TitleObserver&) = delete;

    // Returns number of times `TitleWasSet` was called.
    int title_change_count() { return title_change_count_; }
    // WebStateObserver overrides:
    void TitleWasSet(WebState* web_state) override { title_change_count_++; }
    void WebStateDestroyed(WebState* web_state) override {
      NOTREACHED_IN_MIGRATION();
    }

   private:
    int title_change_count_ = 0;
  };

  TitleObserver observer;
  base::ScopedObservation<WebState, WebStateObserver> scoped_observer(
      &observer);
  scoped_observer.Observe(web_state());
  ASSERT_EQ(0, observer.title_change_count());

  // Expect TitleWasSet callback after the page is loaded and due to WKWebView
  // title change KVO. Title updates happen asynchronously, so wait until the
  // title is updated.
  LoadHtml(@"<title>Title1</title>");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return web_state()->GetTitle() == u"Title1";
  }));
  EXPECT_EQ(2, observer.title_change_count());

  // Expect at least one more TitleWasSet callback after changing title via
  // JavaScript. On iOS 10 WKWebView fires 3 callbacks after JS excucution
  // with the following title changes: "Title2", "" and "Title2".
  // TODO(crbug.com/40508196): There should be only 2 calls of TitleWasSet.
  // Fix expecteation when WKWebView stops sending extra KVO calls.
  ExecuteJavaScript(@"window.document.title = 'Title2';");
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return web_state()->GetTitle() == u"Title2";
  }));
  EXPECT_GE(observer.title_change_count(), 3);
}

// Tests that fragment change navigations use title from the previous page.
TEST_F(CRWWebControllerTitleTest, FragmentChangeNavigationsUsePreviousTitle) {
  LoadHtml(@"<title>Title1</title>");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return web_state()->GetTitle() == u"Title1";
  }));
  ExecuteJavaScript(@"window.location.hash = '#1'");
  EXPECT_EQ(u"Title1", web_state()->GetTitle());
}

// Test fixture for JavaScript execution.
class ScriptExecutionTest : public WebTestWithWebController {
 protected:
  // Calls `executeUserJavaScript:completionHandler:`, waits for script
  // execution completion, and synchronously returns the result.
  id ExecuteUserJavaScript(NSString* java_script, NSError** error) {
    __block id script_result = nil;
    __block NSError* script_error = nil;
    __block bool script_executed = false;
    [web_controller()
        executeUserJavaScript:java_script
            completionHandler:^(id local_result, NSError* local_error) {
              script_result = local_result;
              script_error = local_error;
              script_executed = true;
            }];

    EXPECT_TRUE(WaitForCondition(^{
      return script_executed;
    }));

    if (error) {
      *error = script_error;
    }
    return script_result;
  }
};

// Tests evaluating user script on an http page.
TEST_F(ScriptExecutionTest, UserScriptOnHttpPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));
  NSError* error = nil;
  EXPECT_NSEQ(@0, ExecuteUserJavaScript(@"window.w = 0;", &error));
  EXPECT_FALSE(error);

  EXPECT_NSEQ(@0, ExecuteJavaScript(@"window.w"));
}

// Tests evaluating user script on app-specific page. Pages with app-specific
// URLs have elevated privileges and JavaScript execution should not be allowed
// for them.
TEST_F(ScriptExecutionTest, UserScriptOnAppSpecificPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));

  // Change last committed URL to app-specific URL.
  NavigationManagerImpl& nav_manager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  nav_manager.AddPendingItem(
      GURL(kTestAppSpecificURL), Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  nav_manager.CommitPendingItem();

  NSError* error = nil;
  EXPECT_FALSE(ExecuteUserJavaScript(@"window.w = 0;", &error));
  ASSERT_TRUE(error);
  EXPECT_NSEQ(kJSEvaluationErrorDomain, error.domain);
  EXPECT_EQ(JS_EVALUATION_ERROR_CODE_REJECTED, error.code);

  EXPECT_FALSE(ExecuteJavaScript(@"window.w"));
}

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest : public WebTestWithWebController {
 protected:
  void SetUp() override {
    WebTestWithWebController::SetUp();
    web_view_ = BuildTerminatedWKWebView();
    CRWFakeWebViewContentView* webViewContentView =
        [[CRWFakeWebViewContentView alloc]
            initWithMockWebView:web_view_
                     scrollView:[web_view_ scrollView]];
    [web_controller() injectWebViewContentView:webViewContentView];

    // This test intentionally crashes the render process.
    SetIgnoreRenderProcessCrashesDuringTesting(true);
  }
  WKWebView* web_view_;
};

// Tests that WebStateDelegate::RenderProcessGone is called when WKWebView web
// process has crashed.
TEST_F(CRWWebControllerWebProcessTest, Crash) {
  ASSERT_TRUE([web_controller() isViewAlive]);
  ASSERT_FALSE([web_controller() isWebProcessCrashed]);
  ASSERT_FALSE(web_state()->IsCrashed());
  ASSERT_FALSE(web_state()->IsEvicted());

  FakeWebStateObserver observer(web_state());
  FakeWebStateObserver* observer_ptr = &observer;
  SimulateWKWebViewCrash(web_view_);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return observer_ptr->render_process_gone_info();
      }));
  EXPECT_EQ(web_state(), observer.render_process_gone_info()->web_state);
  EXPECT_FALSE([web_controller() isViewAlive]);
  EXPECT_TRUE([web_controller() isWebProcessCrashed]);
  EXPECT_TRUE(web_state()->IsCrashed());
  EXPECT_TRUE(web_state()->IsEvicted());
}

// Tests that WebState is considered as evicted but not crashed when calling
// SetWebUsageEnabled(false).
TEST_F(CRWWebControllerWebProcessTest, Eviction) {
  ASSERT_TRUE([web_controller() isViewAlive]);
  ASSERT_FALSE([web_controller() isWebProcessCrashed]);
  ASSERT_FALSE(web_state()->IsCrashed());
  ASSERT_FALSE(web_state()->IsEvicted());

  web_state()->SetWebUsageEnabled(false);
  EXPECT_FALSE([web_controller() isViewAlive]);
  EXPECT_FALSE([web_controller() isWebProcessCrashed]);
  EXPECT_FALSE(web_state()->IsCrashed());
  EXPECT_TRUE(web_state()->IsEvicted());
}

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebViewTest : public WebTestWithWebController {
 protected:
  void SetUp() override {
    WebTestWithWebController::SetUp();

    web_view_ = [[CRWFakeWKWebViewObserverCount alloc] init];
    CRWFakeWebViewContentView* webViewContentView =
        [[CRWFakeWebViewContentView alloc]
            initWithMockWebView:web_view_
                     scrollView:web_view_.scrollView];
    [web_controller() injectWebViewContentView:webViewContentView];
  }
  CRWFakeWKWebViewObserverCount* web_view_;
};

// Tests that the KVO for the WebView are removed when the WebState is
// destroyed. See crbug.com/1002786.
TEST_F(CRWWebControllerWebViewTest, CheckNoKVOWhenWebStateDestroyed) {
  // Load a first URL.
  NSURL* URL = [NSURL URLWithString:@"about:blank"];
  NSURLRequest* request = [NSURLRequest requestWithURL:URL];
  [web_view_ loadRequest:request];
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      TestTimeouts::action_timeout(), ^bool() {
        return !web_view_.loading;
      }));

  // Destroying the WebState should call stop at a point where all observers are
  // supposed to be removed.
  DestroyWebState();

  EXPECT_FALSE(web_view_.hadObserversWhenStopping);
}

}  // namespace web
