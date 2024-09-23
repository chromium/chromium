// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"

namespace content {
namespace {

class StorageServiceSandboxBrowserTest : public ContentBrowserTest {
 public:
  StorageServiceSandboxBrowserTest() = default;

  DOMStorageContextWrapper* dom_storage() {
    auto* partition =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition());
    return partition->GetDOMStorageContext();
  }

  void WaitForAnyLocalStorageDataAsync(base::OnceClosure callback) {
    dom_storage()->GetLocalStorageControl()->GetUsage(base::BindOnce(
        [](StorageServiceSandboxBrowserTest* test, base::OnceClosure callback,
           std::vector<storage::mojom::StorageUsageInfoPtr> usage) {
          if (!usage.empty()) {
            std::move(callback).Run();
            return;
          }

          base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&StorageServiceSandboxBrowserTest::
                                 WaitForAnyLocalStorageDataAsync,
                             base::Unretained(test), std::move(callback)),
              base::Milliseconds(50));
        },
        this, std::move(callback)));
  }

  void WaitForAnyLocalStorageData() {
    base::RunLoop loop;
    WaitForAnyLocalStorageDataAsync(loop.QuitClosure());
    loop.Run();
  }

  mojo::Remote<storage::mojom::TestApi>& GetTestApi() {
    if (!test_api_) {
      StoragePartitionImpl::GetStorageServiceForTesting()->BindTestApi(
          test_api_.BindNewPipeAndPassReceiver().PassPipe());
    }
    return test_api_;
  }

 private:
  mojo::Remote<storage::mojom::TestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(StorageServiceSandboxBrowserTest, BasicLaunch) {
  // Basic smoke test to ensure that we can launch the Storage Service in a
  // sandboxed and it won't crash immediately.
  GetTestApi().FlushForTesting();
  EXPECT_TRUE(GetTestApi().is_connected());
}

IN_PROC_BROWSER_TEST_F(StorageServiceSandboxBrowserTest, PRE_DomStorage) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  std::ignore =
      EvalJs(shell()->web_contents(), R"(window.localStorage.yeet = 42)");
  WaitForAnyLocalStorageData();
}

IN_PROC_BROWSER_TEST_F(StorageServiceSandboxBrowserTest, DomStorage) {
  // Tests that Local Storage data persists from the PRE test setup above,
  // providing basic assurance that the sandboxed process is able to manipulate
  // filesystem contents as needed.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "empty.html")));
  EXPECT_EQ("42",
            EvalJs(shell()->web_contents(), R"(window.localStorage.yeet)"));
}

// TODO(crbug.com/40835229): Fix and enable the test on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_CompactDatabase DISABLED_CompactDatabase
#else
#define MAYBE_CompactDatabase CompactDatabase
#endif
IN_PROC_BROWSER_TEST_F(StorageServiceSandboxBrowserTest,
                       MAYBE_CompactDatabase) {
  // Tests that the sandboxed service can execute a LevelDB database compaction
  // operation without crashing. If the service crashes, the sync call below
  // will return false.
  mojo::ScopedAllowSyncCallForTesting allow_sync_calls;
  EXPECT_TRUE(
      GetTestApi()->ForceLeveldbDatabaseCompaction("CompactDatabaseTestDb"));
}

}  // namespace
}  // namespace content
