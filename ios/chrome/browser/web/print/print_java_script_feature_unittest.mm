// A Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/print/print_java_script_feature.h"

#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/print/web_state_printer.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#include "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"

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
@end

class PrintJavaScriptFeatureTest : public ChromeWebTest {
 protected:
  PrintJavaScriptFeatureTest()
      : ChromeWebTest(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    ChromeWebTest::SetUp();

    printer_ = [[PrintJavaScriptFeatureTestPrinter alloc] init];
    GetWebClient()->SetJavaScriptFeatures({&feature_});

    PrintTabHelper::CreateForWebState(web_state());
    PrintTabHelper::FromWebState(web_state())->set_printer(printer_);
  }

  web::FakeWebClient* GetWebClient() override {
    return static_cast<web::FakeWebClient*>(
        WebTestWithWebState::GetWebClient());
  }

  PrintJavaScriptFeatureTestPrinter* printer_;
  PrintJavaScriptFeature feature_;
};

TEST_F(PrintJavaScriptFeatureTest, PrintInvoked) {
  ASSERT_FALSE(printer_.printInvoked);

  ASSERT_TRUE(LoadHtml(kButtonPageHtml));
  ASSERT_TRUE(web::test::TapWebViewElementWithId(web_state(), "button"));
  EXPECT_TRUE(printer_.printInvoked);
}
