// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
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

  MOCK_METHOD1(OnDownloadCompleted, void(const base::FilePath& file_path));
  MOCK_METHOD0(OnDownloadAborted, void());

 private:
  ~MockDownloadFileObserver() override {}

  DISALLOW_COPY_AND_ASSIGN(MockDownloadFileObserver);
};

class DragDownloadFileTest : public ContentBrowserTest {
 public:
  DragDownloadFileTest() {}
  ~DragDownloadFileTest() override {}

  void Succeed() {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
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

 private:
  base::ScopedTempDir downloads_directory_;

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFileTest);
};

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_NetError) {
  base::FilePath name(downloads_directory().AppendASCII(
      "DragDownloadFileTest_NetError.txt"));
  GURL url = embedded_test_server()->GetURL("/download/download-test.lib");
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(name, base::File(), url,
                                                 referrer, referrer_encoding,
                                                 shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer.get(), OnDownloadAborted())
      .WillOnce(InvokeWithoutArgs(this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer.get(), OnDownloadCompleted(_))
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  file->Start(observer.get());
  RunMessageLoop();
}

IN_PROC_BROWSER_TEST_F(DragDownloadFileTest, DragDownloadFileTest_Complete) {
  base::FilePath name(downloads_directory().AppendASCII(
        "DragDownloadFileTest_Complete.txt"));
  GURL url = embedded_test_server()->GetURL("/download/download-test.lib");
  Referrer referrer;
  std::string referrer_encoding;
  auto file = std::make_unique<DragDownloadFile>(name, base::File(), url,
                                                 referrer, referrer_encoding,
                                                 shell()->web_contents());
  scoped_refptr<MockDownloadFileObserver> observer(
      new MockDownloadFileObserver());
  EXPECT_CALL(*observer.get(), OnDownloadCompleted(_))
      .WillOnce(InvokeWithoutArgs(this, &DragDownloadFileTest::Succeed));
  ON_CALL(*observer.get(), OnDownloadAborted())
      .WillByDefault(InvokeWithoutArgs(this, &DragDownloadFileTest::FailFast));
  file->Start(observer.get());
  RunMessageLoop();
}

// TODO(benjhayden): Test Stop().

}  // namespace content
