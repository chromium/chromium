// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/storage_service.mojom.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {
namespace {

class StorageServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  StorageServiceRestartBrowserTest() {
    // These tests only make sense when the service is running out-of-process.
    feature_list_.InitAndEnableFeature(features::kStorageServiceOutOfProcess);
  }

  DOMStorageContextWrapper* dom_storage() {
    auto* partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    return partition->GetDOMStorageContext();
  }

  void WaitForAnyLocalStorageDataAsync(base::OnceClosure callback) {
    dom_storage()->GetLocalStorageControl()->GetUsage(base::BindOnce(
        [](StorageServiceRestartBrowserTest* test, base::OnceClosure callback,
           std::vector<storage::mojom::LocalStorageUsageInfoPtr> usage) {
          if (!usage.empty()) {
            std::move(callback).Run();
            return;
          }

          base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&StorageServiceRestartBrowserTest::
                                 WaitForAnyLocalStorageDataAsync,
                             base::Unretained(test), std::move(callback)),
              base::TimeDelta::FromMilliseconds(50));
        },
        this, std::move(callback)));
  }

  void WaitForAnyLocalStorageData() {
    base::RunLoop loop;
    WaitForAnyLocalStorageDataAsync(loop.QuitClosure());
    loop.Run();
  }

  void FlushLocalStorage() {
    base::RunLoop loop;
    dom_storage()->GetLocalStorageControl()->Flush(loop.QuitClosure());
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
  base::test::ScopedFeatureList feature_list_;
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
  ignore_result(
      EvalJs(shell()->web_contents(), R"(setSessionStorageValue("foo", 42))"));

  // Note that for Session Storage we don't need to wait for a commit. This is
  // racy, but that's the point: whether or not a commit happens in time, the
  // renderer should always retain its local cache of stored values.

  CrashStorageServiceAndWaitForRestart();
  EXPECT_EQ("42", EvalJs(shell()->web_contents(),
                         R"(getSessionStorageValue("foo"))"));
}

// Flaky on Linux, Windows, and Mac. See crbug.com/1066138.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN) || \
    defined(OS_MAC)
#define MAYBE_LocalStorageRecovery DISABLED_LocalStorageRecovery
#else
#define MAYBE_LocalStorageRecovery LocalStorageRecovery
#endif
IN_PROC_BROWSER_TEST_F(StorageServiceRestartBrowserTest,
                       MAYBE_LocalStorageRecovery) {
  // Tests that the Local Storage API can recover and continue normal operation
  // after a Storage Service crash.
  EXPECT_TRUE(
      NavigateToURL(shell(), GetTestUrl("dom_storage", "crash_recovery.html")));
  ignore_result(
      EvalJs(shell()->web_contents(), R"(setLocalStorageValue("foo", 42))"));

  // We wait for the above storage request to be fully committed to disk. This
  // ensures that renderer gets the correct value when recovering from the
  // impending crash.
  WaitForAnyLocalStorageData();
  FlushLocalStorage();

  CrashStorageServiceAndWaitForRestart();
  EXPECT_EQ("42",
            EvalJs(shell()->web_contents(), R"(getLocalStorageValue("foo"))"));
}

}  // namespace
}  // namespace content
