// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/print/print_java_script_feature.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/print/web_state_printer.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kButtonPageHtml[] =
    "<html><body>"
    "<button id=\"button\" onclick=\"window.print()\">BUTTON</button>"
    "</body></html>";
}

@interface PrintJavaScriptFeatureTestPrinter : NSObject <WebStatePrinter>
@property(nonatomic, readwrite) BOOL printInvoked;
@end

@implementation PrintJavaScriptFeatureTestPrinter
- (void)printWebState:(web::WebState*)webState {
  self.printInvoked = YES;
}

- (void)printWebState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController {
  self.printInvoked = YES;
}
@end

class PrintJavaScriptFeatureTest : public PlatformTest {
 protected:
  PrintJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    printer_ = [[PrintJavaScriptFeatureTestPrinter alloc] init];
    GetWebClient()->SetJavaScriptFeatures({&feature_});

    PrintTabHelper::CreateForWebState(web_state());
    PrintTabHelper::FromWebState(web_state())->set_printer(printer_);
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  PrintJavaScriptFeatureTestPrinter* printer_;
  PrintJavaScriptFeature feature_;
};

TEST_F(PrintJavaScriptFeatureTest, PrintInvoked) {
  ASSERT_FALSE(printer_.printInvoked);

  web::test::LoadHtml(base::SysUTF8ToNSString(kButtonPageHtml), web_state());

  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "button"));
  EXPECT_TRUE(printer_.printInvoked);
}
