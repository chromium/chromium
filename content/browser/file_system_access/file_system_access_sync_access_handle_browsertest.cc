// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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
  EXPECT_EQ(true, EvalJs(browser, R"(
    runOnWorkerAndWaitForResult(`
      const root = await navigator.storage.getDirectory();
      const fh = await root.getFileHandle(
          'test_oversized_write', {create: true});
      const ah = await fh.createSyncAccessHandle();
      // 32769 Wasm pages * 64KiB = 2,147,549,184 bytes (INT_MAX + 65537).
      const mem = new WebAssembly.Memory({ initial: 32769 });
      const buffer = new Uint8Array(mem.buffer);
      let threwTypeError = false;
      try {
        ah.write(buffer);
      } catch (e) {
        if (e instanceof TypeError) threwTypeError = true;
      }
      const size = ah.getSize();
      ah.close();
      await root.removeEntry('test_oversized_write');
      return threwTypeError && size === 0;
    `);
  )"));
}
#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace content
