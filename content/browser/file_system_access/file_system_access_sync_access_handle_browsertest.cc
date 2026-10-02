// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_settings.h"

namespace content {

// Browser tests for FileSystemSyncAccessHandle.
class FileSystemAccessSyncAccessHandleBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kContentShellUserDataDir,
                                   temp_dir_.GetPath());
  }

 private:
  base::ScopedTempDir temp_dir_;
};

// This test requires allocating >INT_MAX bytes via WebAssembly.Memory,
// which is only possible on 64-bit architectures.
#if defined(ARCH_CPU_64_BITS)
IN_PROC_BROWSER_TEST_F(FileSystemAccessSyncAccessHandleBrowserTest,
                       WriteRejectsOversizedBuffer) {
  const GURL& test_url =
      embedded_test_server()->GetURL("/run_async_code_on_worker.html");
  Shell* browser = CreateBrowser();

  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  // Allocate a >INT_MAX buffer via WebAssembly.Memory (which bypasses
  // PartitionAlloc's ~2GB cap on ArrayBuffer), then call write().
  // The write() must throw a TypeError and leave the file empty.
  EXPECT_EQ("RangeError", EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      const root = await navigator.storage.getDirectory();
      const fh = await root.getFileHandle(
          'test_oversized_write', {create: true});
      const ah = await fh.createSyncAccessHandle();
      // 32769 Wasm pages * 64KiB = 2,147,549,184 bytes (INT_MAX + 65537).
      const mem = new WebAssembly.Memory({ initial: 32769 });
      const buffer = new Uint8Array(mem.buffer);
      let thrown_error = "did not throw";
      try {
        ah.write(buffer);
      } catch (e) {
        thrown_error = e.constructor.name;
      }
      const size = ah.getSize();
      ah.close();
      await root.removeEntry('test_oversized_write');
      if (size !== 0) {
        return "size is not 0";
      }
      return thrown_error;
    `);
  )"));
}
#endif  // defined(ARCH_CPU_64_BITS)

// Exercise quota errors in regular and off-the-record contexts; the latter
// uses the incognito file delegate.
class FileSystemAccessSyncAccessHandleQuotaBrowserTest
    : public FileSystemAccessSyncAccessHandleBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    FileSystemAccessSyncAccessHandleBrowserTest::SetUpOnMainThread();
    browser_ = GetParam() ? CreateOffTheRecordBrowser() : CreateBrowser();
    scoped_refptr<storage::QuotaManager> quota_manager =
        browser_->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetQuotaManager();
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([quota_manager] {
          quota_manager->SetQuotaSettings(storage::QuotaSettings(
              /*pool_size=*/50000000, /*per_storage_key_quota=*/10000000,
              /*should_remain_available=*/0, /*must_remain_available=*/0));
        }),
        run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_TRUE(NavigateToURL(browser_, embedded_test_server()->GetURL(
                                            "/run_async_code_on_worker.html")));
  }

  void TearDownOnMainThread() override {
    browser_ = nullptr;
    FileSystemAccessSyncAccessHandleBrowserTest::TearDownOnMainThread();
  }

 protected:
  Shell* browser() { return browser_; }

 private:
  raw_ptr<Shell> browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(FileSystemAccessSyncAccessHandleQuotaBrowserTest,
                       WriteThrowsQuotaExceededError) {
  EXPECT_EQ("QuotaExceededError", EvalJs(browser(), R"(
    runOnWorkerAndWaitForResult(`
      const root = await navigator.storage.getDirectory();
      const file = await root.getFileHandle('quota_write', {create: true});
      const handle = await file.createSyncAccessHandle();
      try {
        if (handle.write(new Uint8Array([1]), {at: 0}) !== 1)
          return 'Small write failed';
        try {
          return handle.write(new Uint8Array(30000000), {at: 0});
        } catch (error) {
          return error.name;
        }
      } finally {
        handle.close();
        await root.removeEntry('quota_write');
      }
    `);
  )"));
}

IN_PROC_BROWSER_TEST_P(FileSystemAccessSyncAccessHandleQuotaBrowserTest,
                       TruncateThrowsQuotaExceededError) {
  EXPECT_EQ("QuotaExceededError", EvalJs(browser(), R"(
    runOnWorkerAndWaitForResult(`
      const root = await navigator.storage.getDirectory();
      const file = await root.getFileHandle('quota_truncate', {create: true});
      const handle = await file.createSyncAccessHandle();
      try {
        handle.truncate(1);
        if (handle.getSize() !== 1)
          return 'Small truncate failed';
        try {
          handle.truncate(30000000);
          return 'Truncate did not throw';
        } catch (error) {
          return error.name;
        }
      } finally {
        handle.close();
        await root.removeEntry('quota_truncate');
      }
    `);
  )"));
}

INSTANTIATE_TEST_SUITE_P(,
                         FileSystemAccessSyncAccessHandleQuotaBrowserTest,
                         testing::Bool());

}  // namespace content
