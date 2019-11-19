// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/open_in/open_in_tab_helper.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An object that conforms to OpenInTabHelperDelegate for testing.
@interface FakeOpenInTabHelperDelegate : NSObject <OpenInTabHelperDelegate>
// URL of the last opened document.
@property(nonatomic, assign) GURL lastOpenedDocumentURL;
// The last suggested file name used for openIn.
@property(nonatomic, copy) NSString* lastSuggestedFileName;
// True if disableOpenInForWebState was called.
@property(nonatomic, assign) BOOL openInDisabled;
// True if destroyOpenInForWebState was called.
@property(nonatomic, assign) BOOL openInDestroyed;
@end

@implementation FakeOpenInTabHelperDelegate
- (void)enableOpenInForWebState:(web::WebState*)webState
                withDocumentURL:(const GURL&)documentURL
              suggestedFileName:(NSString*)suggestedFileName {
  self.lastOpenedDocumentURL = documentURL;
  self.lastSuggestedFileName = suggestedFileName;
  self.openInDisabled = NO;
}
- (void)disableOpenInForWebState:(web::WebState*)webState {
  self.openInDisabled = YES;
}
- (void)destroyOpenInForWebState:(web::WebState*)webState {
  self.openInDestroyed = YES;
}
@end

namespace {

const char kContentDispositionWithFileName[] =
    "attachment; filename=\"suggested_filename.pdf\"";
const char kPdfContentType[] = "application/pdf";
const char kInvalidFileNameUrl[] = "https://test.test/";
const char kValidFileNameUrl[] = "https://test.test/file_name.pdf";
const char kContentDispositionWithoutFileName[] =
    "attachment; parameter=parameter_value";
const char kHtmlContentType[] = "text/html";
}  // namespace

// Test fixture for OpenInTabHelper class.
class OpenInTabHelperTest : public PlatformTest {
 protected:
  OpenInTabHelperTest()
      : delegate_([[FakeOpenInTabHelperDelegate alloc] init]) {
    OpenInTabHelper::CreateForWebState(&web_state_);
    tab_helper()->SetDelegate(delegate_);

    // Setup navigation manager.
    std::unique_ptr<web::TestNavigationManager> navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

  OpenInTabHelper* tab_helper() {
    return OpenInTabHelper::FromWebState(&web_state_);
  }

  // Simulates a navigation to |url| and set the proper response headers based
  // on |content_type| and |content_disposition|
  void NavigateTo(const GURL& url,
                  const char* content_type,
                  const char* content_disposition) {
    item_ = web::NavigationItem::Create();
    item_->SetURL(url);
    scoped_refptr<net::HttpResponseHeaders> headers =
        new net::HttpResponseHeaders("HTTP 1.1 200 OK");
    headers->AddHeader(base::StringPrintf("Content-Type: %s", content_type));
    headers->AddHeader(
        base::StringPrintf("Content-Disposition: %s", content_disposition));

    web::FakeNavigationContext navigation_context;
    navigation_context.SetResponseHeaders(headers);
    web_state_.OnNavigationStarted(&navigation_context);
    navigation_manager_->SetLastCommittedItem(item_.get());
    // PageLoaded will always be called after DidFinishNavigation.
    web_state_.OnNavigationFinished(&navigation_context);
    web_state_.LoadData(nil, base::SysUTF8ToNSString(content_type), url);
  }

  FakeOpenInTabHelperDelegate* delegate_ = nil;
  web::TestWebState web_state_;
  web::TestNavigationManager* navigation_manager_;
  std::unique_ptr<web::NavigationItem> item_;
};

// Tests that on starting new navigation openIn will be disabled.
TEST_F(OpenInTabHelperTest, WebStateObservationStartNavigation) {
  ASSERT_FALSE(delegate_.openInDisabled);
  web_state_.OnNavigationStarted(nullptr);
  EXPECT_TRUE(delegate_.openInDisabled);
}

// Tests that on web state destruction openIn will be destroyed.
TEST_F(OpenInTabHelperTest, WebStateObservationDestruction) {
  std::unique_ptr<web::TestWebState> web_state =
      std::make_unique<web::TestWebState>();
  OpenInTabHelper::CreateForWebState(web_state.get());
  OpenInTabHelper::FromWebState(web_state.get())->SetDelegate(delegate_);
  EXPECT_FALSE(delegate_.openInDestroyed);
  web_state = nullptr;
  EXPECT_TRUE(delegate_.openInDestroyed);
}

// Tests that openIn is enabled for PDF documents and that it uses the file name
// from the content desposition key in the response headers.
TEST_F(OpenInTabHelperTest, OpenInForPDFWithFileNameFromContentDesposition) {
  ASSERT_FALSE(delegate_.openInDisabled);

  GURL url(kValidFileNameUrl);
  NavigateTo(url, kPdfContentType, kContentDispositionWithFileName);
  EXPECT_FALSE(delegate_.openInDisabled);
  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);
  EXPECT_NSEQ(@"suggested_filename.pdf", delegate_.lastSuggestedFileName);
}

// Tests that openIn is enabled for PDF documents and that it uses the file name
// from the URL if the content desposition key in the response headers doesn't
// have file name.
TEST_F(OpenInTabHelperTest, OpenInForPDFWithFileNameFromURL) {
  ASSERT_FALSE(delegate_.openInDisabled);

  GURL url(kValidFileNameUrl);
  NavigateTo(url, kPdfContentType, kContentDispositionWithoutFileName);
  EXPECT_FALSE(delegate_.openInDisabled);

  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);
  EXPECT_NSEQ(@"file_name.pdf", delegate_.lastSuggestedFileName);
}

// Tests that openIn is enabled for PDF documents and that it uses the default
// file name if neither the URL nor the content desposition key in the response
// headers has a file name.
TEST_F(OpenInTabHelperTest, OpenInForPDFWithDefaultFileName) {
  ASSERT_FALSE(delegate_.openInDisabled);

  GURL url(kInvalidFileNameUrl);
  NavigateTo(url, kPdfContentType, kContentDispositionWithoutFileName);
  EXPECT_FALSE(delegate_.openInDisabled);
  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);
  std::string default_file_name =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE) + ".pdf";

  EXPECT_NSEQ(base::SysUTF8ToNSString(default_file_name),
              delegate_.lastSuggestedFileName);
}

// Tests that openIn is disabled for non PDF documents.
TEST_F(OpenInTabHelperTest, OpenInDisabledForNonPDF) {
  ASSERT_FALSE(delegate_.openInDisabled);
  GURL url(kValidFileNameUrl);
  NavigateTo(url, kHtmlContentType, kContentDispositionWithoutFileName);
  EXPECT_EQ(GURL::EmptyGURL(), delegate_.lastOpenedDocumentURL);
  EXPECT_FALSE(delegate_.lastSuggestedFileName);
  EXPECT_TRUE(delegate_.openInDisabled);
}
