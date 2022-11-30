// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/open_in/open_in_tab_helper.h"

#import <memory>

#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/http/http_response_headers.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

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

const char kInvalidFileNameUrl[] = "https://test.test/";
const char kContentDispositionWithoutFileName[] =
    "attachment; parameter=parameter_value";
const char kHtmlContentType[] = "text/html";
const char kValidFileNamePDF[] = "https://test.test/file_name.pdf";
const char kValidFileNameMicrosoftPowerPointOpenXML[] =
    "https://test.test/file_name.pptx";

// Returns the content type according to the current testing value.
std::string ContentTypeForMimeType(OpenInMimeType parameter) {
  switch (parameter) {
    case OpenInMimeType::kMimeTypeMicrosoftPowerPointOpenXML:
      return content_type::kMimeTypeMicrosoftPowerPointOpenXML;
    case OpenInMimeType::kMimeTypeMicrosoftWordOpenXML:
      return content_type::kMimeTypeMicrosoftWordOpenXML;
    case OpenInMimeType::kMimeTypeMicrosoftExcelOpenXML:
      return content_type::kMimeTypeMicrosoftExcelOpenXML;
    case OpenInMimeType::kMimeTypePDF:
      return content_type::kMimeTypePDF;
    case OpenInMimeType::kMimeTypeMicrosoftWord:
      return content_type::kMimeTypeMicrosoftWord;
    case OpenInMimeType::kMimeTypeJPEG:
      return content_type::kMimeTypeJPEG;
    case OpenInMimeType::kMimeTypePNG:
      return content_type::kMimeTypePNG;
    case OpenInMimeType::kMimeTypeMicrosoftPowerPoint:
      return content_type::kMimeTypeMicrosoftPowerPoint;
    case OpenInMimeType::kMimeTypeRTF:
      return content_type::kMimeTypeRTF;
    case OpenInMimeType::kMimeTypeSVG:
      return content_type::kMimeTypeSVG;
    case OpenInMimeType::kMimeTypeMicrosoftExcel:
      return content_type::kMimeTypeMicrosoftExcel;
    // Should not be reached.
    case OpenInMimeType::kMimeTypeNotHandled:
      return "";
  }
}

// Returns the file extension according to the current testing value.
std::string ExtensionForMimeType(OpenInMimeType parameter) {
  switch (parameter) {
    case OpenInMimeType::kMimeTypeMicrosoftPowerPointOpenXML:
      return ".pptx";
    case OpenInMimeType::kMimeTypeMicrosoftWordOpenXML:
      return ".docx";
    case OpenInMimeType::kMimeTypeMicrosoftExcelOpenXML:
      return ".xlsx";
    case OpenInMimeType::kMimeTypePDF:
      return ".pdf";
    case OpenInMimeType::kMimeTypeMicrosoftWord:
      return ".doc";
    case OpenInMimeType::kMimeTypeJPEG:
      return ".jpeg";
    case OpenInMimeType::kMimeTypePNG:
      return ".png";
    case OpenInMimeType::kMimeTypeMicrosoftPowerPoint:
      return ".ppt";
    case OpenInMimeType::kMimeTypeRTF:
      return ".rtf";
    case OpenInMimeType::kMimeTypeSVG:
      return ".svg";
    case OpenInMimeType::kMimeTypeMicrosoftExcel:
      return ".xls";
    // Should not be reached.
    case OpenInMimeType::kMimeTypeNotHandled:
      return "";
  }
}

// Returns the file name according to the current testing value.
std::string FileNameForMimeType(OpenInMimeType parameter) {
  return "filename" + ExtensionForMimeType(parameter);
}
}  // namespace

// Test fixture for OpenInTabHelper class.
class OpenInTabHelperTest
    : public PlatformTest,
      public ::testing::WithParamInterface<OpenInMimeType> {
 protected:
  OpenInTabHelperTest()
      : delegate_([[FakeOpenInTabHelperDelegate alloc] init]) {
    OpenInTabHelper::CreateForWebState(&web_state_);
    tab_helper()->SetDelegate(delegate_);

    // Setup navigation manager.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

  OpenInTabHelper* tab_helper() {
    return OpenInTabHelper::FromWebState(&web_state_);
  }

  // Simulates a navigation to `url` and set the proper response headers based
  // on `content_type` and `content_disposition`
  void NavigateTo(const GURL& url,
                  const char* content_type,
                  const char* content_disposition) {
    item_ = web::NavigationItem::Create();
    item_->SetURL(url);
    scoped_refptr<net::HttpResponseHeaders> headers =
        new net::HttpResponseHeaders("HTTP 1.1 200 OK");
    headers->SetHeader("Content-Type", content_type);
    headers->SetHeader("Content-Disposition", content_disposition);

    web::FakeNavigationContext navigation_context;
    navigation_context.SetResponseHeaders(headers);
    web_state_.OnNavigationStarted(&navigation_context);
    navigation_manager_->SetLastCommittedItem(item_.get());
    // PageLoaded will always be called after DidFinishNavigation.
    web_state_.OnNavigationFinished(&navigation_context);
    web_state_.LoadData(nil, base::SysUTF8ToNSString(content_type), url);
  }

  FakeOpenInTabHelperDelegate* delegate_ = nil;
  web::FakeWebState web_state_;
  web::FakeNavigationManager* navigation_manager_;
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
  auto web_state = std::make_unique<web::FakeWebState>();
  OpenInTabHelper::CreateForWebState(web_state.get());
  OpenInTabHelper::FromWebState(web_state.get())->SetDelegate(delegate_);
  EXPECT_FALSE(delegate_.openInDestroyed);
  web_state = nullptr;
  EXPECT_TRUE(delegate_.openInDestroyed);
}

// Tests that openIn is enabled for exportable files and that it uses the file
// name from the content desposition key in the response headers.
TEST_P(OpenInTabHelperTest,
       OpenInForExportableFilesWithFileNameFromContentDesposition) {
  base::test::ScopedFeatureList feature_list;
  ASSERT_FALSE(delegate_.openInDisabled);

  const std::string file_name =
      FileNameForMimeType(OpenInTabHelperTest::GetParam());
  GURL url("https://test.test/" + file_name);

  NavigateTo(url,
             ContentTypeForMimeType(OpenInTabHelperTest::GetParam()).c_str(),
             ("attachment; filename=\"suggested_" + file_name + "\"").c_str());

  EXPECT_FALSE(delegate_.openInDisabled);
  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);

  std::string suggested_file_name = "suggested_" + file_name;
  EXPECT_NSEQ(base::SysUTF8ToNSString(suggested_file_name),
              delegate_.lastSuggestedFileName);
}

// Tests that openIn is enabled for exportable files and that it uses the file
// name from the URL if the content desposition key in the response headers
// doesn't have file name.
TEST_P(OpenInTabHelperTest, OpenInForExportableFilesWithFileNameFromURL) {
  base::test::ScopedFeatureList feature_list;
  ASSERT_FALSE(delegate_.openInDisabled);

  const std::string file_name =
      FileNameForMimeType(OpenInTabHelperTest::GetParam());
  GURL url("https://test.test/" + file_name);
  NavigateTo(url,
             ContentTypeForMimeType(OpenInTabHelperTest::GetParam()).c_str(),
             kContentDispositionWithoutFileName);

  EXPECT_FALSE(delegate_.openInDisabled);
  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);

  EXPECT_NSEQ(base::SysUTF8ToNSString(file_name),
              delegate_.lastSuggestedFileName);
}

// Tests that openIn is enabled for exportable files and that it uses the
// default file name if neither the URL nor the content desposition key in the
// response headers has a file name.
TEST_P(OpenInTabHelperTest, OpenInForExportableFilesWithDefaultFileName) {
  base::test::ScopedFeatureList feature_list;
  ASSERT_FALSE(delegate_.openInDisabled);

  GURL url(kInvalidFileNameUrl);
  NavigateTo(url,
             ContentTypeForMimeType(OpenInTabHelperTest::GetParam()).c_str(),
             kContentDispositionWithoutFileName);

  EXPECT_FALSE(delegate_.openInDisabled);
  EXPECT_EQ(url, delegate_.lastOpenedDocumentURL);

  std::string default_file_name =
      l10n_util::GetStringUTF8(IDS_IOS_OPEN_IN_FILE_DEFAULT_TITLE) +
      ExtensionForMimeType(OpenInTabHelperTest::GetParam());

  EXPECT_NSEQ(base::SysUTF8ToNSString(default_file_name),
              delegate_.lastSuggestedFileName);
}

// Tests that openIn is disabled for non exportable files.
TEST_F(OpenInTabHelperTest, OpenInDisabledForNonExportableFiles) {
  base::test::ScopedFeatureList feature_list;
  ASSERT_FALSE(delegate_.openInDisabled);

  // Testing PDF.
  GURL url_pdf(kValidFileNamePDF);
  NavigateTo(url_pdf, kHtmlContentType, kContentDispositionWithoutFileName);

  EXPECT_EQ(GURL::EmptyGURL(), delegate_.lastOpenedDocumentURL);
  EXPECT_FALSE(delegate_.lastSuggestedFileName);
  EXPECT_TRUE(delegate_.openInDisabled);

  // Testing Microsoft PowerPoint OpenXML.
  GURL url_pptx(kValidFileNameMicrosoftPowerPointOpenXML);
  NavigateTo(url_pptx, kHtmlContentType, kContentDispositionWithoutFileName);

  EXPECT_EQ(GURL::EmptyGURL(), delegate_.lastOpenedDocumentURL);
  EXPECT_FALSE(delegate_.lastSuggestedFileName);
  EXPECT_TRUE(delegate_.openInDisabled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    OpenInTabHelperTest,
    ::testing::Values(OpenInMimeType::kMimeTypePDF,
                      OpenInMimeType::kMimeTypeMicrosoftWord,
                      OpenInMimeType::kMimeTypeMicrosoftWordOpenXML,
                      OpenInMimeType::kMimeTypeJPEG,
                      OpenInMimeType::kMimeTypePNG,
                      OpenInMimeType::kMimeTypeMicrosoftPowerPoint,
                      OpenInMimeType::kMimeTypeMicrosoftPowerPointOpenXML,
                      OpenInMimeType::kMimeTypeRTF,
                      OpenInMimeType::kMimeTypeSVG,
                      OpenInMimeType::kMimeTypeMicrosoftExcel,
                      OpenInMimeType::kMimeTypeMicrosoftExcelOpenXML));
