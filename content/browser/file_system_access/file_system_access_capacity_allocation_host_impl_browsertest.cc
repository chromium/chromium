// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace content {

// This browser test implements end-to-end tests for
// FileSystemAccessCapacityAllocationHostImpl.
class FileSystemAccessCapacityAllocationHostImplBrowserTest
    : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                   temp_dir_.GetPath());
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  int64_t GetUsageSync(scoped_refptr<storage::QuotaManager> quota_manager,
                       blink::StorageKey storage_key) {
    int64_t current_usage;
    base::RunLoop run_loop;
    RunOnIOThreadBlocking(base::BindOnce(
        &storage::QuotaManager::GetUsageAndQuota, quota_manager, storage_key,
        blink::mojom::StorageType::kTemporary,
        base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode result,
                                       int64_t usage, int64_t quota) {
          current_usage = usage;
          run_loop.Quit();
        })));
    run_loop.Run();
    return current_usage;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessCapacityAllocationHostImplBrowserTest,
                       QuotaUsageAfterClosing) {
  const GURL& test_url =
      embedded_test_server()->GetURL("/run_async_code_on_worker.html");
  Shell* browser = CreateBrowser();
  scoped_refptr<storage::QuotaManager> quota_manager =
      browser->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetQuotaManager();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(test_url.spec());

  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_closing', {create: true});
      return true;
    `);
  )")
                  .ExtractBool());
  int64_t usage_before_operation = GetUsageSync(quota_manager, storage_key);

  EXPECT_TRUE(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_closing', {create: false});
      let ah = await fh.createSyncAccessHandle();
      await ah.truncate(100);
      await ah.truncate(10);
      await ah.close();
      return true;
    `);
  )")
                  .ExtractBool());
  int64_t usage_after_operation = GetUsageSync(quota_manager, storage_key);
  EXPECT_EQ(usage_before_operation + 10, usage_after_operation);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessCapacityAllocationHostImplBrowserTest,
                       QuotaUsageAfterForNonemptyFile) {
  const GURL& test_url =
      embedded_test_server()->GetURL("/run_async_code_on_worker.html");
  Shell* browser = CreateBrowser();
  scoped_refptr<storage::QuotaManager> quota_manager =
      browser->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetQuotaManager();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(test_url.spec());

  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_existing', {create: true});
      let ah =  await fh.createSyncAccessHandle();
      await ah.truncate(100);
      await ah.close();
      return true;
    `);
  )")
                  .ExtractBool());
  int64_t usage_before_operation = GetUsageSync(quota_manager, storage_key);

  ReloadBypassingCacheBlockUntilNavigationsComplete(
      browser, /*number_of_navigations=*/1);

  EXPECT_TRUE(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_existing', {create: false});
      let ah = await fh.createSyncAccessHandle();
      await ah.truncate(0);
      await ah.close();
      return true;
    `);
  )")
                  .ExtractBool());
  int64_t usage_after_operation = GetUsageSync(quota_manager, storage_key);
  EXPECT_EQ(usage_before_operation, usage_after_operation + 100);
}

// TODO(crbug.com/1304977): Failing on Mac builders.
#if BUILDFLAG(IS_MAC)
#define MAYBE_QuotaUsageOverallocation DISABLED_QuotaUsageOverallocation
#else
#define MAYBE_QuotaUsageOverallocation QuotaUsageOverallocation
#endif
IN_PROC_BROWSER_TEST_F(FileSystemAccessCapacityAllocationHostImplBrowserTest,
                       MAYBE_QuotaUsageOverallocation) {
  // TODO(https://crbug.com/1240056): Implement a more sophisticated test suite
  // for this feature.
  const GURL& test_url =
      embedded_test_server()->GetURL("/run_async_code_on_worker.html");
  Shell* browser = CreateBrowser();
  scoped_refptr<storage::QuotaManager> quota_manager =
      browser->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetQuotaManager();
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting(test_url.spec());

  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_EQ(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_file_small', {create: true});
      let ah =  await fh.createSyncAccessHandle();
      let storage_manager = await navigator.storage.estimate();
      let usage_before_operation = storage_manager.usageDetails.fileSystem;
      await ah.truncate(100);
      storage_manager = await navigator.storage.estimate();
      let usage_after_operation = storage_manager.usageDetails.fileSystem;
      await ah.close();
      return usage_after_operation-usage_before_operation;
    `);
  )")
                .ExtractInt(),
            1024 * 1024);
  EXPECT_EQ(EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      let root = await navigator.storage.getDirectory();
      let fh = await root.getFileHandle('test_file_medium', {create: true});
      let ah =  await fh.createSyncAccessHandle();
      let storage_manager = await navigator.storage.estimate();
      let usage_before_operation = storage_manager.usageDetails.fileSystem;
      let new_file_size = 3*1024*1024;
      await ah.truncate(new_file_size);
      storage_manager = await navigator.storage.estimate();
      let usage_after_operation = storage_manager.usageDetails.fileSystem;
      await ah.close();
      return usage_after_operation-usage_before_operation;
    `);
  )")
                .ExtractInt(),
            4 * 1024 * 1024);
}

}  // namespace content
