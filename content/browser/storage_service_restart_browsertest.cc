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

namespace content {
namespace {

class StorageServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  StorageServiceRestartBrowserTest() = default;

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
        [](StorageServiceRestartBrowserTest* test, base::OnceClosure callback,
           std::vector<storage::mojom::StorageUsageInfoPtr> usage) {
          if (!usage.empty()) {
            std::move(callback).Run();
            return;
          }

          base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&StorageServiceRestartBrowserTest::
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

  void CrashStorageServiceAndWaitForRestart() {
    mojo::Remote<storage::mojom::StorageService>& service =
        StoragePartitionImpl::GetStorageServiceForTesting();
    base::RunLoop loop;
    service.set_disconnect_handler(base::BindLambdaForTesting([&] {
      loop.Quit();
      service.reset();
    }));
    GetTestApi()->CrashNow();
    loop.Run();
    test_api_.reset();
  }

 private:
  mojo::Remote<storage::mojom::TestApi> test_api_;
};

IN_PROC_BROWSER_TEST_F(StorageServiceRestartBrowserTest, BasicReconnect) {
  // Basic smoke test to ensure that we can force-crash the service and
  // StoragePartitionImpl will internally re-establish a working connection to
  // a new process.
  GetTestApi().FlushForTesting();
  EXPECT_TRUE(GetTestApi().is_connected());
  CrashStorageServiceAndWaitForRestart();
  GetTestApi().FlushForTesting();
  EXPECT_TRUE(GetTestApi().is_connected());
}

IN_PROC_BROWSER_TEST_F(StorageServiceRestartBrowserTest,
                       SessionStorageRecovery) {
  // Tests that the Session Storage API can recover and continue normal
  // operation after a Storage Service crash.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("dom_storage", "crash_recovery.html")));
  std::ignore =
      EvalJs(shell()->web_contents(), R"(setSessionStorageValue("foo", 42))");

  // Note that for Session Storage we don't need to wait for a commit. This is
  // racy, but that's the point: whether or not a commit happens in time, the
  // renderer should always retain its local cache of stored values.

  CrashStorageServiceAndWaitForRestart();
  EXPECT_EQ("42", EvalJs(shell()->web_contents(),
                         R"(getSessionStorageValue("foo"))"));
}

IN_PROC_BROWSER_TEST_F(StorageServiceRestartBrowserTest, LocalStorageRecovery) {
  // Tests that the Local Storage API can recover and continue normal operation
  // after a Storage Service crash.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("dom_storage", "crash_recovery.html")));
  std::ignore =
      EvalJs(shell()->web_contents(), R"(setLocalStorageValue("foo", 42))");

  WaitForAnyLocalStorageData();

  CrashStorageServiceAndWaitForRestart();

  // Unlike Session Storage, Local Storage clobbers its renderer-side cache when
  // the backend connection is lost. Thus, whether the data still exists depends
  // on whether it managed to be flushed to disk before crashing, which is
  // unpredictable.
  EvalJsResult result =
      EvalJs(shell()->web_contents(), R"(getLocalStorageValue("foo"))");
  ASSERT_THAT(result, content::EvalJsResult::IsOk());
  EXPECT_TRUE(result.value.GetString().empty() ||
              result.value.GetString() == "42");

  // Local Storage should resume working as expected after the service is
  // restarted.
  std::ignore =
      EvalJs(shell()->web_contents(), R"(setLocalStorageValue("foo", 420))");
  EXPECT_EQ("420",
            EvalJs(shell()->web_contents(), R"(getLocalStorageValue("foo"))"));
}

}  // namespace
}  // namespace content
