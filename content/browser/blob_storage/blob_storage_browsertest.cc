// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {
namespace {
const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 10;

const size_t kTestBlobStorageMaxBlobMemorySize = 200;
const uint64_t kTestBlobStorageMaxDiskSpace = 3000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 20;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 50;

void SetBlobLimitsOnIO(scoped_refptr<ChromeBlobStorageContext> context,
                       const storage::BlobStorageLimits& limits) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(context);
  CHECK(context->context());
  context->context()->set_limits_for_testing(limits);
}

}  // namespace

// This browser test is aimed towards exercising the blob storage transportation
// strategies and paging memory to disk.
class BlobStorageBrowserTest : public ContentBrowserTest {
 public:
  BlobStorageBrowserTest() {
    limits_.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits_.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits_.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits_.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits_.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits_.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits_.max_file_size = kTestBlobStorageMaxFileSizeBytes;
  }

  scoped_refptr<ChromeBlobStorageContext> GetBlobContext() {
    return ChromeBlobStorageContext::GetFor(
        shell()->web_contents()->GetBrowserContext());
  }

  void SetBlobLimits() {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&SetBlobLimitsOnIO, GetBlobContext(),
                                  std::cref(limits_)));
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on blob storage, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();
    ASSERT_TRUE(the_browser);

    VLOG(0) << "Navigating to URL and blocking. " << test_url.spec();
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

 protected:
  storage::BlobStorageLimits limits_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlobStorageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BlobStorageBrowserTest, BlobCombinations) {
  SetBlobLimits();
  SimpleTest(GetTestUrl("blob_storage", "blob_creation_and_slicing.html"));

  auto blob_context = GetBlobContext();
  base::RunLoop loop;
  base::PostTask(
      FROM_HERE, {BrowserThread::IO}, base::BindLambdaForTesting([&]() {
        const storage::BlobMemoryController& memory_controller =
            blob_context->context()->memory_controller();
        // Our exact usages depend on IPC message ordering & garbage collection.
        // Since this is basically random, we just check bounds.
        EXPECT_LT(0u, memory_controller.memory_usage());
        EXPECT_LT(0ul, memory_controller.disk_usage());
        EXPECT_GT(memory_controller.disk_usage(),
                  static_cast<uint64_t>(memory_controller.memory_usage()));
        EXPECT_GT(limits_.max_blob_in_memory_space,
                  memory_controller.memory_usage());
        EXPECT_GT(limits_.effective_max_disk_space,
                  memory_controller.disk_usage());

        loop.Quit();
      }));
  loop.Run();

  shell()->Close();

  // Make sure we run all file / io tasks.
  base::RunLoop().RunUntilIdle();
  RunAllTasksUntilIdle();
}

}  // namespace content
