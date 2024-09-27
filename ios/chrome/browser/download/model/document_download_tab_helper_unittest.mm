// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/document_download_tab_helper.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/download/model/document_download_tab_helper_metrics.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/download_mimetype_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for testing DocumentDownloadTabHelperTest class.
class DocumentDownloadTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    download_manager_delegate_ =
        [[FakeDownloadManagerTabHelperDelegate alloc] init];
    DownloadManagerTabHelper::CreateForWebState(&web_state_);
    DownloadManagerTabHelper::FromWebState(&web_state_)
        ->SetDelegate(download_manager_delegate_);

    DocumentDownloadTabHelper::CreateForWebState(&web_state_);
    web_state_.SetBrowserState(profile_.get());
    web_state_.WasShown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeDownloadManagerTabHelperDelegate* download_manager_delegate_;
  web::FakeWebState web_state_;
};

// Tests that loading a PDF will trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, DownloadPDF) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);

  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadMimeType,
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadSizeInMB, 0, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kNoConflict, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadStateAtNavigation,
                                      DocumentDownloadState::kNotStarted, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadFinalState,
                                      DocumentDownloadState::kNotStarted, 1);
}

// Tests that loading an HTML page will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, NoPDFNoDownload) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("text/html");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(true);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(nullptr, download_manager_delegate_.state);
  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadMimeType, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadSizeInMB, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadConflictResolution, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadStateAtNavigation, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadFinalState, 0);
}

// Tests that loading a video page will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, VideoNoDownload) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("video/mp4");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(nullptr, download_manager_delegate_.state);

  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadMimeType, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadSizeInMB, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadConflictResolution, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadStateAtNavigation, 0);
  histogram_tester.ExpectTotalCount(kIOSDocumentDownloadFinalState, 0);
}

// Tests that loading an audio file will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, AudioDownload) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("audio/mpeg");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);

  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadMimeType,
                                      DownloadMimeTypeResult::MP3Audio, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadSizeInMB, 0, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kNoConflict, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadStateAtNavigation,
                                      DocumentDownloadState::kNotStarted, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadFinalState,
                                      DocumentDownloadState::kNotStarted, 1);
}

// Tests that Content-Length is parsed correctly.
TEST_F(DocumentDownloadTabHelperTest, ContentLengthParsing) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web::FakeNavigationContext context;
  context.SetResponseHeaders(net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 12345678\r\n\r\n"));
  web_state_.OnNavigationFinished(&context);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);
  ASSERT_NE(nullptr, download_manager_delegate_.currentDownloadTask);
  EXPECT_EQ(12345678,
            download_manager_delegate_.currentDownloadTask->GetTotalBytes());

  web::FakeNavigationContext new_navigation_context;
  web_state_.OnNavigationStarted(&new_navigation_context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadMimeType,
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadSizeInMB,
                                      12345678 / 1024 / 1024, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kNoConflict, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadStateAtNavigation,
                                      DocumentDownloadState::kNotStarted, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadFinalState,
                                      DocumentDownloadState::kNotStarted, 1);
}

// Tests that default content length is -1.
TEST_F(DocumentDownloadTabHelperTest, NoContentLengthParsing) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web::FakeNavigationContext context;
  context.SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n"
                                            "Accept-Language: en\r\n\r\n"));
  web_state_.OnNavigationFinished(&context);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);
  ASSERT_NE(nullptr, download_manager_delegate_.currentDownloadTask);
  EXPECT_EQ(-1,
            download_manager_delegate_.currentDownloadTask->GetTotalBytes());

  web::FakeNavigationContext new_navigation_context;
  web_state_.OnNavigationStarted(&new_navigation_context);
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadSizeInMB, 0, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadMimeType,
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kNoConflict, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadStateAtNavigation,
                                      DocumentDownloadState::kNotStarted, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadFinalState,
                                      DocumentDownloadState::kNotStarted, 1);
}

// Tests that delegate is asked to follow fullscreen when download is not
// started.
TEST_F(DocumentDownloadTabHelperTest, FollowFullscreen) {
  base::HistogramTester histogram_tester;
  // Create a first task so the task created for download the document starts
  // pending.
  DownloadManagerTabHelper* download_manager =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL("https://foo.bar"),
                                                      "text/html");
  web::FakeDownloadTask* task_ptr = task.get();
  download_manager->SetCurrentDownload(std::move(task));

  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  ASSERT_EQ(task_ptr, download_manager_delegate_.currentDownloadTask);
  EXPECT_EQ(GURL("https://foo.bar"),
            download_manager_delegate_.currentDownloadTask->GetOriginalUrl());
  EXPECT_EQ(nullptr, download_manager_delegate_.decidingPolicyForDownload);

  EXPECT_EQ(NO, download_manager_delegate_.shouldObserveFullscreen);

  download_manager_delegate_.currentDownloadTask->Cancel();
  task_environment_.RunUntilIdle();

  ASSERT_NE(nullptr, download_manager_delegate_.currentDownloadTask);
  EXPECT_EQ(GURL("https://foo.test"),
            download_manager_delegate_.currentDownloadTask->GetOriginalUrl());
  EXPECT_EQ(YES, download_manager_delegate_.shouldObserveFullscreen);

  download_manager_delegate_.currentDownloadTask->Start(
      base::FilePath("/tmp/test"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(NO, download_manager_delegate_.shouldObserveFullscreen);
}

// Tests that user cancelling previous task is logged correctly.
TEST_F(DocumentDownloadTabHelperTest, ConflictLoggingCancel) {
  base::HistogramTester histogram_tester;
  // Create a first task so the task created for download the document starts
  // pending.
  DownloadManagerTabHelper* download_manager =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL("https://foo.bar"),
                                                      "text/html");
  download_manager->SetCurrentDownload(std::move(task));

  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  download_manager_delegate_.currentDownloadTask->Cancel();
  task_environment_.RunUntilIdle();

  web::FakeNavigationContext new_navigation_context;
  web_state_.OnNavigationStarted(&new_navigation_context);

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kPreviousDownloadWasCancelled, 1);
}

// Tests that previous task completing is logged correctly.
TEST_F(DocumentDownloadTabHelperTest, ConflictLoggingComplete) {
  base::HistogramTester histogram_tester;
  // Create a first task so the task created for download the document starts
  // pending.
  DownloadManagerTabHelper* download_manager =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL("https://foo.bar"),
                                                      "text/html");
  web::FakeDownloadTask* task_ptr = task.get();
  download_manager->SetCurrentDownload(std::move(task));

  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  task_ptr->SetState(web::DownloadTask::State::kComplete);
  task_environment_.RunUntilIdle();
  download_manager_delegate_.currentDownloadTask->Cancel();
  task_environment_.RunUntilIdle();

  web::FakeNavigationContext new_navigation_context;
  web_state_.OnNavigationStarted(&new_navigation_context);

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kPreviousDownloadCompleted, 1);
}

// Tests that unresolved conflict is logged correctly.
TEST_F(DocumentDownloadTabHelperTest, ConflictLoggingNotFinishing) {
  base::HistogramTester histogram_tester;
  // Create a first task so the task created for download the document starts
  // pending.
  DownloadManagerTabHelper* download_manager =
      DownloadManagerTabHelper::FromWebState(&web_state_);
  auto task = std::make_unique<web::FakeDownloadTask>(GURL("https://foo.bar"),
                                                      "text/html");
  download_manager->SetCurrentDownload(std::move(task));

  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  web::FakeNavigationContext new_navigation_context;
  web_state_.OnNavigationStarted(&new_navigation_context);

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kPreviousDownloadDidNotFinish, 1);
}

// Tests the state of download tasks created from `DocumentDownloadTabHelper`
// are recorded accordingly.
TEST_F(DocumentDownloadTabHelperTest, StateLogging) {
  base::HistogramTester histogram_tester;
  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);

  download_manager_delegate_.currentDownloadTask->Start(
      base::FilePath("/tmp/test"));
  task_environment_.RunUntilIdle();

  web::FakeNavigationContext context;
  web_state_.OnNavigationStarted(&context);
  task_environment_.RunUntilIdle();

  download_manager_delegate_.currentDownloadTask->Cancel();
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadMimeType,
      DownloadMimeTypeResult::AdobePortableDocumentFormat, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadSizeInMB, 0, 1);
  histogram_tester.ExpectUniqueSample(
      kIOSDocumentDownloadConflictResolution,
      DocumentDownloadConflictResolution::kNoConflict, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadStateAtNavigation,
                                      DocumentDownloadState::kInProgress, 1);
  histogram_tester.ExpectUniqueSample(kIOSDocumentDownloadFinalState,
                                      DocumentDownloadState::kCancelled, 1);
}
