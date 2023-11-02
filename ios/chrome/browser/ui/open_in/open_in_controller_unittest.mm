// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller.h"
#import "ios/chrome/browser/ui/open_in/open_in_controller_testing.h"
#import "ios/chrome/browser/ui/open_in/open_in_histograms.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class OpenInControllerTest : public PlatformTest {
 public:
  OpenInControllerTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~OpenInControllerTest() override { [open_in_controller_ detachFromWebState]; }

  void SetUp() override {
    PlatformTest::SetUp();

    // Set up the directory it downloads the file to.
    // Note that the value of kDocumentsTempPath must match the one in
    // open_in_controller.mm
    {
      NSString* const kDocumentsTempPath = @"OpenIn";
      NSString* tempDirPath = [NSTemporaryDirectory()
          stringByAppendingPathComponent:kDocumentsTempPath];
      base::FilePath directory(base::SysNSStringToUTF8(tempDirPath));
      EXPECT_TRUE(base::CreateDirectory(directory));
    }

    GURL documentURL = GURL("http://www.test.com/doc.pdf");
    base_view_controller = [[UIViewController alloc] init];
    open_in_controller_ = [[OpenInController alloc]
        initWithBaseViewController:base_view_controller
                  URLLoaderFactory:test_shared_url_loader_factory_
                          webState:&web_state_
                           browser:nullptr];
    [open_in_controller_ enableWithDocumentURL:documentURL
                             suggestedFilename:@"doc.pdf"];
  }

  // Returns the string of a two blank pages created PDF document.
  std::string CreatePdfString() {
    NSMutableData* pdf_data = [NSMutableData data];
    UIGraphicsBeginPDFContextToData(pdf_data, CGRectZero, nil);
    UIGraphicsBeginPDFPage();
    UIGraphicsBeginPDFPage();
    UIGraphicsEndPDFContext();
    return std::string(reinterpret_cast<char const*>([pdf_data bytes]),
                       pdf_data.length);
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  OpenInController* open_in_controller_;
  UIViewController* base_view_controller;
  base::HistogramTester histogram_tester_;
  web::FakeWebState web_state_;
};

TEST_F(OpenInControllerTest, TestDisplayOpenInMenu) {
  histogram_tester_.ExpectTotalCount(kOpenInDownloadHistogram, 0);

  id vc_partial_mock = OCMPartialMock(base_view_controller);
  [[vc_partial_mock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* viewController) {
        if ([viewController isKindOfClass:[UIActivityViewController class]]) {
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  [open_in_controller_ startDownload];

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  std::string pdf_str = CreatePdfString();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), pdf_str);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(kOpenInDownloadHistogram,
                                      static_cast<base::HistogramBase::Sample>(
                                          OpenInDownloadResult::kSucceeded),
                                      1);
  histogram_tester_.ExpectTotalCount(kOpenInDownloadHistogram, 1);

  EXPECT_OCMOCK_VERIFY(vc_partial_mock);
}

TEST_F(OpenInControllerTest, TestCorruptedPDFDownload) {
  histogram_tester_.ExpectTotalCount(kOpenInDownloadHistogram, 0);

  id vc_partial_mock = OCMPartialMock(base_view_controller);
  [[vc_partial_mock reject] presentViewController:[OCMArg any]
                                         animated:YES
                                       completion:nil];

  [open_in_controller_ startDownload];

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  std::string pdf_str = CreatePdfString();
  // Only use half the string so the downloaded PDF is corrupted.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), pdf_str.substr(pdf_str.size() / 2));
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      kOpenInDownloadHistogram,
      static_cast<base::HistogramBase::Sample>(OpenInDownloadResult::kFailed),
      1);
  histogram_tester_.ExpectTotalCount(kOpenInDownloadHistogram, 1);
  EXPECT_OCMOCK_VERIFY(vc_partial_mock);
}

}  // namespace
