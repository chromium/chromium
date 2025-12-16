// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
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
#include "content/shell/browser/shell.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager.h"

using storage::QuotaManager;

namespace content {

// This browser test is aimed towards exercising the File System API bindings
// and the actual implementation that lives in the browser side.
class FileSystemBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  FileSystemBrowserTest() {
    feature_list_.InitWithFeatureState(storage::features::kStaticStorageQuota,
                                       should_report_static_quota());
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    browser_ = is_incognito() ? CreateOffTheRecordBrowser() : shell();
  }

  void TearDownOnMainThread() override { browser_ = nullptr; }

  void SimpleTest(const GURL& test_url) {
    // The test page will perform tests on FileAPI, then navigate to either
    // a #pass or #fail ref.
    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(browser(), test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        browser()->web_contents()->GetLastCommittedURL().GetRef();
    if (result != "pass") {
      std::string js_result = EvalJs(browser(), "getLog()").ExtractString();
      FAIL() << "Failed: " << js_result;
    }
  }

  int64_t GetTotalDiskSpaceOr(int64_t default_value) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::SysInfo::AmountOfTotalDiskSpace(
               browser()->web_contents()->GetBrowserContext()->GetPath())
        .value_or(default_value);
  }

  Shell* browser() { return browser_; }

  bool is_incognito() { return testing::get<0>(GetParam()); }
  bool should_report_static_quota() { return testing::get<1>(GetParam()); }

 protected:
  bool is_incognito_;
  raw_ptr<Shell> browser_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FileSystemBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

class FileSystemBrowserTestWithLowQuota : public FileSystemBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FileSystemBrowserTest::SetUpOnMainThread();
    SetLowQuota(browser()
                    ->web_contents()
                    ->GetBrowserContext()
                    ->GetDefaultStoragePartition()
                    ->GetQuotaManager());
  }

  static void SetLowQuota(scoped_refptr<QuotaManager> qm) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));

    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          DCHECK_CURRENTLY_ON(BrowserThread::IO);
          // These sizes must correspond with expectations in html and js.
          const int kMeg = 1000 * 1024;
          storage::QuotaSettings settings;
          settings.pool_size = 25 * kMeg;
          settings.per_storage_key_quota = 5 * kMeg;
          settings.must_remain_available = 10 * kMeg;
          settings.refresh_interval = base::TimeDelta::Max();
          qm->SetQuotaSettings(settings);
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         FileSystemBrowserTestWithLowQuota,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTest, RequestTest) {
  SimpleTest(embedded_test_server()->GetURL("/fileapi/request_test.html"));
}

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTest, CreateTest) {
  SimpleTest(embedded_test_server()->GetURL("/fileapi/create_test.html"));
}

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTestWithLowQuota, QuotaTest) {
  const int64_t kMeg = 1000 * 1024;
  const int64_t kGiB = 1024 * 1024 * 1024;

  // Reported quota is the lower of 10 GiB or disk size rounded up to the
  // nearest 1 GiB.
  int64_t disk_size = GetTotalDiskSpaceOr(INT64_MAX);
  if (disk_size <= 0) {
    FAIL() << "Disk size[" << disk_size << "] was reported negative or zero.";
  }
  int64_t disk_quota_GiB = std::min(int64_t{10}, (disk_size - 1) / kGiB + 1);

  // Incognito reports a static quota of 1 GiB because it calculates on ram, not
  // on disk size. TODO(crbug.com/464484739): Set to disk_quota_GiB when fixed.
  const int64_t quota = should_report_static_quota()
                            ? (is_incognito() ? 1 : disk_quota_GiB) * kGiB
                            : 5 * kMeg;

  SimpleTest(embedded_test_server()->GetURL("/fileapi/quota_test.html?quota=" +
                                            base::NumberToString(quota)));
}

}  // namespace content
