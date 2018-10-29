// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/open_in_controller_testing.h"
#include "ios/web/public/test/test_web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kOpenInDownloadResultHistogram[] = "IOS.OpenIn.DownloadResult";

class OpenInControllerTest : public PlatformTest {
 public:
  OpenInControllerTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void TearDown() override { PlatformTest::TearDown(); }

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
    parent_view_ = [[UIView alloc] init];
    id webController = [OCMockObject niceMockForClass:[CRWWebController class]];
    open_in_controller_ = [[OpenInController alloc]
        initWithURLLoaderFactory:test_shared_url_loader_factory_
                   webController:webController];
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

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

  net::TestURLFetcherFactory factory_;
  OpenInController* open_in_controller_;
  UIView* parent_view_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OpenInControllerTest, TestDisplayOpenInMenu) {
  histogram_tester_.ExpectTotalCount(kOpenInDownloadResultHistogram, 0);
  id document_controller =
      [OCMockObject niceMockForClass:[UIDocumentInteractionController class]];
  [open_in_controller_ setDocumentInteractionController:document_controller];
  [open_in_controller_ startDownload];
  [[[document_controller expect] andReturnValue:@YES]
      presentOpenInMenuFromRect:CGRectZero
                         inView:OCMOCK_ANY
                       animated:YES];

  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  // Set the response for the set URLFetcher to be a blank PDF.
  std::string pdf_str = CreatePdfString();
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), pdf_str);
  scoped_task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(kOpenInDownloadResultHistogram,
                                      static_cast<base::HistogramBase::Sample>(
                                          OpenInDownloadResult::kSucceeded),
                                      1);
  histogram_tester_.ExpectTotalCount(kOpenInDownloadResultHistogram, 1);
  EXPECT_OCMOCK_VERIFY(document_controller);
}

TEST_F(OpenInControllerTest, TestCorruptedPDFDownload) {
  histogram_tester_.ExpectTotalCount(kOpenInDownloadResultHistogram, 0);
  id document_controller =
      [OCMockObject niceMockForClass:[UIDocumentInteractionController class]];
  [open_in_controller_ setDocumentInteractionController:document_controller];
  [open_in_controller_ startDownload];
  [[[document_controller reject] andReturnValue:@YES]
      presentOpenInMenuFromRect:CGRectZero
                         inView:OCMOCK_ANY
                       animated:YES];
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  std::string pdf_str = CreatePdfString();
  // Only use half the string so the downloaded PDF is corrupted.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), pdf_str.substr(pdf_str.size() / 2));
  scoped_task_environment_.RunUntilIdle();
  histogram_tester_.ExpectBucketCount(
      kOpenInDownloadResultHistogram,
      static_cast<base::HistogramBase::Sample>(OpenInDownloadResult::kFailed),
      1);
  histogram_tester_.ExpectTotalCount(kOpenInDownloadResultHistogram, 1);
  EXPECT_OCMOCK_VERIFY(document_controller);
}

}  // namespace
