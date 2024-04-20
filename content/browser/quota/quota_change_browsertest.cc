// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <tuple>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/thread_test_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/quota/quota_availability.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

using storage::QuotaAvailability;
using storage::QuotaManager;

namespace {

constexpr int64_t kMBytes = 1024 * 1024;

QuotaAvailability GetVolumeInfoForStoragePressure(const base::FilePath& path) {
  return QuotaAvailability((int64_t)(100 * kMBytes), (int64_t)(2 * kMBytes));
}

}  // namespace

namespace content {

// This browser test is aimed towards exercising the quotachange event bindings
// and the implementation that lives in the browser side.
class QuotaChangeBrowserTest : public ContentBrowserTest,
                               public testing::WithParamInterface<bool> {
 public:
  QuotaChangeBrowserTest() : is_incognito_(GetParam()) {
    feature_list_.InitAndEnableFeature(
        storage::features::kStoragePressureEvent);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/40133191): Remove this when the QuotaChange
    // RuntimeEnabledFeature becomes "stable".
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Disable the notification's rate limiting mechanism.
    command_line->AppendSwitchASCII(switches::kQuotaChangeEventInterval, "0");
  }

  // Posts a task that causes the quota system to call
  // GetVolumeInfoForStoragePressure and eventually fire a QuotaChange event.
  void TriggerStoragePressureCheck(const GURL& test_url) {
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &QuotaManager::GetUsageAndQuotaForWebApps, quota_manager(),
            blink::StorageKey::CreateFromStringForTesting(test_url.spec()),
            blink::mojom::StorageType::kTemporary, base::DoNothing()));
  }

  Shell* browser() {
    if (!browser_) {
      browser_ = is_incognito() ? CreateOffTheRecordBrowser() : shell();
    }
    return browser_;
  }

  bool is_incognito() const { return is_incognito_; }

  QuotaManager* quota_manager() {
    return browser()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetQuotaManager();
  }

 private:
  bool is_incognito_;
  raw_ptr<Shell, DanglingUntriaged> browser_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, QuotaChangeBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(QuotaChangeBrowserTest, DispatchEvent) {
  // TODO(crbug.com/40721281): Implement this test for incognito contexts after
  // device information collection is unified into a single code path within
  // quota.
  if (is_incognito()) {
    return;
  }

  quota_manager()->SetGetVolumeInfoFnForTesting(
      &GetVolumeInfoForStoragePressure);
  auto test_url =
      embedded_test_server()->GetURL("/storage/quota_change_test.html");
  // The test page will perform tests on QuotaManagerHost, then navigate to
  // either a #pass or #fail ref.
  NavigateToURLBlockUntilNavigationsComplete(browser(), test_url,
                                             /*number_of_navigations=*/1);

  TestNavigationObserver observer(browser()->web_contents(),
                                  /*number_of_navigations=*/1);
  TriggerStoragePressureCheck(test_url);
  observer.WaitForNavigationFinished();
  const GURL& last_url = browser()->web_contents()->GetLastCommittedURL();
  if (last_url.ref() != "pass") {
    std::string js_result = EvalJs(browser(), "getLog()").ExtractString();
    FAIL() << "Failed: " << last_url << "\n" << js_result;
  }
}

}  // namespace content
