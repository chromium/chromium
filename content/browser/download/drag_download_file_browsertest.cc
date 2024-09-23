// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_file.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace content {

class MockDownloadFileObserver : public ui::DownloadFileObserver {
 public:
  MockDownloadFileObserver() {}

  MockDownloadFileObserver(const MockDownloadFileObserver&) = delete;
  MockDownloadFileObserver& operator=(const MockDownloadFileObserver&) = delete;

  MOCK_METHOD1(OnDownloadCompleted, void(const base::FilePath& file_path));
  MOCK_METHOD0(OnDownloadAborted, void());

 private:
  ~MockDownloadFileObserver() override {}
};

class DragDownloadFileTest : public ContentBrowserTest {
 public:
  DragDownloadFileTest() = default;

  DragDownloadFileTest(const DragDownloadFileTest&) = delete;
  DragDownloadFileTest& operator=(const DragDownloadFileTest&) = delete;

  void Succeed() {
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(quit_closure_));
  }

  void FailFast() {
    CHECK(false);
  }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()->web_contents()->GetBrowserContext()
            ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory());
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  const base::FilePath& downloads_directory() const {
    return downloads_directory_.GetPath();
  }

  void RunUntilSucceed() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::ScopedTempDir downloads_directory_;
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_NetError) {
  base::FilePath name(downloads_directory().AppendASCII(
      "DragDownloadFileTest_NetError.txt"));
  GURL url = embedded_test_server()->GetURL("/download/download-test.lib");
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(
      name, base::File(), url, referrer, referrer_encoding, std::nullopt,
      shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer.get(), OnDownloadAborted())
      .WillOnce(InvokeWithoutArgs(this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer.get(), OnDownloadCompleted(_))
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  file->Start(observer.get());
  RunUntilSucceed();
}

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_Complete) {
  base::FilePath name(downloads_directory().AppendASCII(
        "DragDownloadFileTest_Complete.txt"));
  GURL url = embedded_test_server()->GetURL("/download/download-test.lib");
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(
      name, base::File(), url, referrer, referrer_encoding, std::nullopt,
      shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer.get(), OnDownloadCompleted(_))
      .WillOnce(InvokeWithoutArgs(this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer.get(), OnDownloadAborted())
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  file->Start(observer.get());
  RunUntilSucceed();
}

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_Initiator) {
  base::FilePath name(
      downloads_directory().AppendASCII("DragDownloadFileTest_Initiator.txt"));
  GURL url = embedded_test_server()->GetURL("/echoheader?sec-fetch-site");
  url::Origin initiator =
      url::Origin::Create(GURL("https://initiator.example.com"));
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(
      name, base::File(), url, referrer, referrer_encoding, initiator,
      shell()->web_contents());
  base::FilePath downloaded_path;
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer.get(), OnDownloadCompleted(_))
      .WillOnce([&](const base::FilePath& file_path) {
        downloaded_path = file_path;
        this->Succeed();
      });
  ON_CALL(*observer.get(), OnDownloadAborted())
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  file->Start(observer.get());
  RunUntilSucceed();

  std::string actual_sec_fetch_site_value;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(
        base::ReadFileToString(downloaded_path, &actual_sec_fetch_site_value));
  }
  EXPECT_EQ("cross-site", actual_sec_fetch_site_value);
}

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_ClosePage) {
  base::FilePath name(
      downloads_directory().AppendASCII("DragDownloadFileTest_Complete.txt"));
  GURL url = embedded_test_server()->GetURL("/download/download-test.lib");
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(
      name, base::File(), url, referrer, referrer_encoding, std::nullopt,
      shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  ON_CALL(*observer.get(), OnDownloadAborted())
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  DownloadManager* manager =
      shell()->web_contents()->GetBrowserContext()->GetDownloadManager();
  file->Start(observer.get());
  shell()->web_contents()->Close();
  RunAllTasksUntilIdle();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  manager->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());
}
// TODO(benjhayden): Test Stop().

}  // namespace content
