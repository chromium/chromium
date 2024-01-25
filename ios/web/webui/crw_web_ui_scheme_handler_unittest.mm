// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_scheme_handler.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web/test/test_url_constants.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
const char kOfflineHost[] = "offline";
const char kChromeScheme[] = "chrome";
}  // namespace

@interface FakeSchemeTask : NSObject <WKURLSchemeTask>

// Override from the protocol to have it readwrite.
@property(nonatomic, readwrite, copy) NSURLRequest* request;

// Response.
@property(nonatomic, readwrite, copy) NSURLResponse* response;

// The error received.
@property(nonatomic, strong) NSError* error;

@property(nonatomic, assign) BOOL receivedData;
@property(nonatomic, assign) BOOL receivedError;

@end

@implementation FakeSchemeTask

@synthesize request = _request;

- (void)didReceiveResponse:(NSURLResponse*)response {
  self.response = response;
}

- (void)didReceiveData:(NSData*)data {
  self.receivedData = YES;
}

- (void)didFinish {
}

- (void)didFailWithError:(NSError*)error {
  self.receivedError = YES;
  self.error = error;
}

#pragma mark - test utils

- (BOOL)responseHasMimetype:(NSString*)mimeType {
  if (![self.response isKindOfClass:NSHTTPURLResponse.class])
    return NO;
  NSHTTPURLResponse* httpResponse =
      static_cast<NSHTTPURLResponse*>(self.response);
  return
      [httpResponse.allHeaderFields[@"Content-Type"] containsString:mimeType];
}
@end

namespace web {

namespace {
class FakeWebUIIOSControllerFactory : public WebUIIOSControllerFactory {
  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override {
    if (!url.SchemeIs(kTestWebUIScheme) && !url.SchemeIs(kChromeScheme))
      return NSURLErrorUnsupportedURL;
    if (url.host() == kOfflineHost)
      return NSURLErrorNotConnectedToInternet;
    return 0;
  }

  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    return nullptr;
  }
};
}  // namespace

// Test fixture for testing CRWWebUISchemeManager.
class CRWWebUISchemeManagerTest : public WebTest {
 public:
  CRWWebUISchemeManagerTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    WebUIIOSControllerFactory::RegisterFactory(&factory_);
  }

  ~CRWWebUISchemeManagerTest() override {
    WebUIIOSControllerFactory::DeregisterFactory(&factory_);
  }

 protected:
  CRWWebUISchemeHandler* CreateSchemeHandler() {
    return [[CRWWebUISchemeHandler alloc]
        initWithURLLoaderFactory:GetSharedURLLoaderFactory()];
  }

  NSURL* GetWebUIURL() {
    NSString* url = [base::SysUTF8ToNSString(kTestWebUIScheme)
        stringByAppendingString:@"://testpage"];
    return [NSURL URLWithString:url];
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_shared_loader_factory_;
  }

  network::TestURLLoaderFactory* GetURLLoaderFactory() {
    return &test_url_loader_factory_;
  }

  void RespondWithData(const GURL& url, const std::string& data) {
    GetURLLoaderFactory()->AddResponse(url.spec(), data);
    base::RunLoop().RunUntilIdle();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  FakeWebUIIOSControllerFactory factory_;
};

// Tests that calling start on the scheme handler returns some data when the URL
// is a WebUI URL.
TEST_F(CRWWebUISchemeManagerTest, StartTaskWithCorrectURL) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:GetWebUIURL()];
  request.mainDocumentURL = GetWebUIURL();
  url_scheme_task.request = request;

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);
}

// Tests that the error returned is the same as the error from the factory.
TEST_F(CRWWebUISchemeManagerTest, ErrorReceived) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  NSURLComponents* components_url = [[NSURLComponents alloc] init];
  components_url.scheme = base::SysUTF8ToNSString(kTestWebUIScheme);
  components_url.host = base::SysUTF8ToNSString(kOfflineHost);
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:components_url.URL];
  request.mainDocumentURL = request.URL;
  url_scheme_task.request = request;

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_FALSE(url_scheme_task.receivedData);
  EXPECT_TRUE(url_scheme_task.receivedError);
  EXPECT_EQ(NSURLErrorNotConnectedToInternet, url_scheme_task.error.code);

  // Check with a different URL.
  request.mainDocumentURL = [NSURL URLWithString:@"invalidScheme://page"];
  url_scheme_task.request = request;
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_FALSE(url_scheme_task.receivedData);
  EXPECT_TRUE(url_scheme_task.receivedError);
  EXPECT_EQ(NSURLErrorUnsupportedURL, url_scheme_task.error.code);
}

// Tests that calling start on the scheme handler returns some data when the URL
// is *not* a WebUI URL but the main document URL is.
TEST_F(CRWWebUISchemeManagerTest, StartTaskWithCorrectMainURL) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:@"https://notAWebUIURL"]];
  request.mainDocumentURL = GetWebUIURL();
  url_scheme_task.request = request;

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);
}

// Tests that calling start on the scheme handler returns an error when the URL
// is correct but the mainDocumentURL is wrong.
TEST_F(CRWWebUISchemeManagerTest, StartTaskWithWrongMainDocumentURL) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:GetWebUIURL()];
  request.mainDocumentURL = [NSURL URLWithString:@"https://notAWebUIURL"];
  url_scheme_task.request = request;

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_FALSE(url_scheme_task.receivedData);
  EXPECT_TRUE(url_scheme_task.receivedError);
}

// Tests that calling stop right after start prevent the handler from returning
// data.
TEST_F(CRWWebUISchemeManagerTest, StopTask) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:GetWebUIURL()];
  request.mainDocumentURL = GetWebUIURL();
  url_scheme_task.request = request;

  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  [scheme_handler webView:web_view stopURLSchemeTask:url_scheme_task];

  RespondWithData(net::GURLWithNSURL(request.URL), "{}");
  EXPECT_FALSE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);
}

// Tests that proper mime-type is returned for a given chrome:// request.
TEST_F(CRWWebUISchemeManagerTest, CheckMimetypeOfChromeScheme) {
  CRWWebUISchemeHandler* scheme_handler = CreateSchemeHandler();
  id web_view = OCMClassMock([WKWebView class]);
  FakeSchemeTask* url_scheme_task = [[FakeSchemeTask alloc] init];

  // Check javascript
  NSMutableURLRequest* request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:@"chrome://clown/res/clown.js"]];
  request.mainDocumentURL = [NSURL URLWithString:@"chrome://clown/"];
  url_scheme_task.request = request;
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  RespondWithData(net::GURLWithNSURL(request.URL), "{}");

  EXPECT_TRUE([url_scheme_task responseHasMimetype:@"text/javascript"]);
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);

  // Check css.
  request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:@"chrome://clown/res/clown.css"]];
  request.mainDocumentURL = [NSURL URLWithString:@"chrome://clown/"];
  url_scheme_task.request = request;
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  RespondWithData(net::GURLWithNSURL(request.URL), "{}");

  EXPECT_TRUE([url_scheme_task responseHasMimetype:@"text/css"]);
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);

  // Check svg.
  request = [NSMutableURLRequest
      requestWithURL:[NSURL URLWithString:@"chrome://clown/res/clown.svg"]];
  request.mainDocumentURL = [NSURL URLWithString:@"chrome://clown/"];
  url_scheme_task.request = request;
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  RespondWithData(net::GURLWithNSURL(request.URL), "{}");

  EXPECT_TRUE([url_scheme_task responseHasMimetype:@"image/svg+xml"]);
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);

  // Anything else, is 'html'.
  request = [NSMutableURLRequest
      requestWithURL:[NSURL
                         URLWithString:@"chrome://clown/res/clown.anything"]];
  request.mainDocumentURL = [NSURL URLWithString:@"chrome://clown/"];
  url_scheme_task.request = request;
  [scheme_handler webView:web_view startURLSchemeTask:url_scheme_task];
  RespondWithData(net::GURLWithNSURL(request.URL), "{}");

  EXPECT_TRUE([url_scheme_task responseHasMimetype:@"text/html"]);
  EXPECT_TRUE(url_scheme_task.receivedData);
  EXPECT_FALSE(url_scheme_task.receivedError);
}

}  // namespace web
