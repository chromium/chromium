// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/document_download_tab_helper.h"

#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
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
    TestChromeBrowserState::Builder test_cbs_builder;
    browser_state_ = test_cbs_builder.Build();

    download_manager_delegate_ =
        [[FakeDownloadManagerTabHelperDelegate alloc] init];
    DownloadManagerTabHelper::CreateForWebState(&web_state_);
    DownloadManagerTabHelper::FromWebState(&web_state_)
        ->SetDelegate(download_manager_delegate_);

    DocumentDownloadTabHelper::CreateForWebState(&web_state_);
    web_state_.SetBrowserState(browser_state_.get());
    web_state_.WasShown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeDownloadManagerTabHelperDelegate* download_manager_delegate_;
  web::FakeWebState web_state_;
};

// Tests that loading a PDF will trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, DownloadPDF) {
  web_state_.SetContentsMimeType("application/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);
}

// Tests that loading an HTML page will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, NoPDFNoDownload) {
  web_state_.SetContentsMimeType("text/html");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(true);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(nullptr, download_manager_delegate_.state);
}

// Tests that loading a video page will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, VideoNoDownload) {
  web_state_.SetContentsMimeType("video/mp4");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_EQ(nullptr, download_manager_delegate_.state);
}

// Tests that loading an audio file will not trigger a download task.
TEST_F(DocumentDownloadTabHelperTest, AudioDownload) {
  web_state_.SetContentsMimeType("audio/mp3");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);
}

// Tests that Content-Length is parsed correctly.
TEST_F(DocumentDownloadTabHelperTest, ContentLengthParsing) {
  web_state_.SetContentsMimeType("document/pdf");
  web_state_.SetCurrentURL(GURL("https://foo.test"));
  web_state_.SetContentIsHTML(false);
  web::FakeNavigationContext context;
  context.SetResponseHeaders(
      net::HttpResponseHeaders::TryToCreate("HTTP/1.1 200 OK\r\n"
                                            "Content-Length: 12345\r\n\r\n"));
  web_state_.OnNavigationFinished(&context);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_NE(nullptr, download_manager_delegate_.state);
  ASSERT_NE(nullptr, download_manager_delegate_.currentDownloadTask);
  EXPECT_EQ(12345,
            download_manager_delegate_.currentDownloadTask->GetTotalBytes());
}

// Tests that default content length is -1.
TEST_F(DocumentDownloadTabHelperTest, NoContentLengthParsing) {
  web_state_.SetContentsMimeType("document/pdf");
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
}

// Tests that delegate is asked to follow fullscreen when download is not
// started.
TEST_F(DocumentDownloadTabHelperTest, FollowFullscreen) {
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
