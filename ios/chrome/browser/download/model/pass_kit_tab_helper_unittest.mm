// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"

#import <memory>

#import <PassKit/PassKit.h>

#import "base/functional/callback_helpers.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/download/model/download_test_util.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/test/fakes/fake_web_content_handler.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/base/io_buffer.h"
#import "net/base/net_errors.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

char kUrl[] = "https://test.test/";

}  // namespace

// Test fixture for testing PassKitTabHelper class.
class PassKitTabHelperTest : public PlatformTest {
 protected:
  PassKitTabHelperTest() : handler_([[FakeWebContentHandler alloc] init]) {
    PassKitTabHelper::GetOrCreateForWebState(&web_state_)
        ->SetWebContentsHandler(handler_);
  }

  PassKitTabHelper* tab_helper() {
    return PassKitTabHelper::GetOrCreateForWebState(&web_state_);
  }

  base::test::TaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  FakeWebContentHandler* handler_;
  base::HistogramTester histogram_tester_;
};

// Tests downloading empty pkpass file.
TEST_F(PassKitTabHelperTest, EmptyBundledFile) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl),
                                                      kPkBundledPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetDone(true);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
        return handler_.called;
      }));

  EXPECT_EQ(1U, handler_.passes.count);
  EXPECT_TRUE([handler_.passes.firstObject isKindOfClass:[NSNull class]]);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadBundledPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kParsingFailure),
      1);
}

// Tests downloading a valid bundle pkpass file.
TEST_F(PassKitTabHelperTest, ValidBundledPassKitFile) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl),
                                                      kPkBundledPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data =
      testing::GetTestFileContents(testing::kBundledPkPassFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];
  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
        return handler_.called;
      }));

  ASSERT_EQ(2U, handler_.passes.count);
  PKPass* first_pass = handler_.passes[0];
  ASSERT_TRUE([first_pass isKindOfClass:[PKPass class]]);
  EXPECT_EQ(PKPassTypeBarcode, first_pass.passType);
  EXPECT_NSEQ(@"pass.com.apple.devpubs.example", first_pass.passTypeIdentifier);
  EXPECT_NSEQ(@"Toy Town", first_pass.organizationName);

  PKPass* second_pass = handler_.passes[1];
  ASSERT_TRUE([second_pass isKindOfClass:[PKPass class]]);
  EXPECT_EQ(PKPassTypeBarcode, second_pass.passType);
  EXPECT_NSEQ(@"pass.google.chrome.test", second_pass.passTypeIdentifier);
  EXPECT_NSEQ(@"Paw Planet", second_pass.organizationName);

  histogram_tester_.ExpectUniqueSample(kUmaDownloadBundledPassKitResult,
                                       static_cast<base::HistogramBase::Sample>(
                                           DownloadPassKitResult::kSuccessful),
                                       1);
}

// Tests downloading a bundle pkpass file containing one valid pass and an
// invalid one.
TEST_F(PassKitTabHelperTest, SemiValidBundledPassKitFile) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl),
                                                      kPkBundledPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data =
      testing::GetTestFileContents(testing::kSemiValidBundledPkPassFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];
  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
        return handler_.called;
      }));

  ASSERT_EQ(1U, handler_.passes.count);
  PKPass* first_pass = handler_.passes[0];
  ASSERT_TRUE([first_pass isKindOfClass:[PKPass class]]);
  EXPECT_EQ(PKPassTypeBarcode, first_pass.passType);
  EXPECT_NSEQ(@"pass.com.apple.devpubs.example", first_pass.passTypeIdentifier);
  EXPECT_NSEQ(@"Toy Town", first_pass.organizationName);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadBundledPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kPartialFailure),
      1);
}

// Tests downloading an invalid bundle pass.
TEST_F(PassKitTabHelperTest, InvalidBundledPassKitFile) {
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl),
                                                      kPkBundledPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  // Use the pass file as bundle pass for invalid data.
  std::string pass_data =
      testing::GetTestFileContents(testing::kPkPassFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];
  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool() {
        return handler_.called;
      }));

  ASSERT_EQ(1U, handler_.passes.count);
  EXPECT_TRUE([handler_.passes.firstObject isKindOfClass:[NSNull class]]);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadBundledPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kParsingFailure),
      1);
}

// Tests downloading empty pkpass file.
TEST_F(PassKitTabHelperTest, EmptyFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetDone(true);
  EXPECT_EQ(1U, handler_.passes.count);
  EXPECT_TRUE([handler_.passes.firstObject isKindOfClass:[NSNull class]]);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kParsingFailure),
      1);
}

// Tests downloading 2 empty pkpass files.
TEST_F(PassKitTabHelperTest, MultipleEmptyFiles) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  auto task2 =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr2 = task2.get();
  tab_helper()->Download(std::move(task2));

  task_ptr->SetDone(true);
  EXPECT_EQ(1U, handler_.passes.count);
  EXPECT_TRUE([handler_.passes.firstObject isKindOfClass:[NSNull class]]);

  task_ptr2->SetDone(true);
  EXPECT_EQ(2U, handler_.passes.count);
  EXPECT_TRUE([handler_.passes.lastObject isKindOfClass:[NSNull class]]);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kParsingFailure),
      2);
}

// Tests downloading a valid pkpass file.
TEST_F(PassKitTabHelperTest, ValidPassKitFile) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));

  std::string pass_data =
      testing::GetTestFileContents(testing::kPkPassFilePath);
  NSData* data = [NSData dataWithBytes:pass_data.data()
                                length:pass_data.size()];
  task_ptr->SetResponseData(data);
  task_ptr->SetDone(true);

  EXPECT_EQ(1U, handler_.passes.count);
  PKPass* pass = handler_.passes.firstObject;
  EXPECT_TRUE([pass isKindOfClass:[PKPass class]]);
  EXPECT_EQ(PKPassTypeBarcode, pass.passType);
  EXPECT_NSEQ(@"pass.com.apple.devpubs.example", pass.passTypeIdentifier);
  EXPECT_NSEQ(@"Toy Town", pass.organizationName);

  histogram_tester_.ExpectUniqueSample(kUmaDownloadPassKitResult,
                                       static_cast<base::HistogramBase::Sample>(
                                           DownloadPassKitResult::kSuccessful),
                                       1);
}

// Tests the change of MIME type during the download. Can happen if the second
// response returned authentication page.
TEST_F(PassKitTabHelperTest, MimeTypeChange) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetMimeType("text/html");
  task_ptr->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kWrongMimeTypeFailure),
      1);
}

// Tests that DownloadPassKitResult::kOtherFailure metric is reported if
// download fails with an error.
TEST_F(PassKitTabHelperTest, DownloadError) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task_ptr->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kOtherFailure),
      1);
}

// Tests that DownloadPassKitResult::kUnauthorizedFailure metric is reported if
// download HTTP response code is 401.
TEST_F(PassKitTabHelperTest, UnauthorizedHttpResponse) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetHttpCode(401);
  task_ptr->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kUnauthorizedFailure),
      1);
}

// Tests that DownloadPassKitResult::kUnauthorizedFailure metric is reported if
// download HTTP response code is 403.
TEST_F(PassKitTabHelperTest, ForbiddenHttpResponse) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kPkPassMimeType);
  web::FakeDownloadTask* task_ptr = task.get();
  tab_helper()->Download(std::move(task));
  task_ptr->SetHttpCode(403);
  task_ptr->SetDone(true);

  histogram_tester_.ExpectUniqueSample(
      kUmaDownloadPassKitResult,
      static_cast<base::HistogramBase::Sample>(
          DownloadPassKitResult::kUnauthorizedFailure),
      1);
}
