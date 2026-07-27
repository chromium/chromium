// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// This browser test is aimed towards exercising the DOMStorage system
// from end-to-end.
class DOMStorageBrowserTest : public base::test::WithFeatureOverride,
                              public ContentBrowserTest {
 public:
  DOMStorageBrowserTest()
      : base::test::WithFeatureOverride(storage::kDomStorageSqlite) {
    // Match the state of `kDomStorageSqliteInMemory` to the top level
    // kDomStorageSqlite. That way in-memory databases (e.g. incognito) will
    // use the backend expected by the param state. The fieldtrial testing
    // config enables kDomStorageSqliteNewDatabases by default for browsertests.
    // Disable it so this parameter controls the on-disk backend.
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{storage::kDomStorageSqliteInMemory},
          /*disabled_features=*/{storage::kDomStorageSqliteNewDatabases});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{storage::kDomStorageSqliteInMemory,
                                 storage::kDomStorageSqliteNewDatabases});
    }
  }

  void SimpleTest(const GURL& test_url, bool incognito) {
    // The test page will perform tests then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().GetRef();
    if (result != "pass") {
      std::string js_result = EvalJs(the_browser, "getLog()").ExtractString();
      FAIL() << "Failed: " << js_result;
    }
  }

  StoragePartition* partition() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

  std::vector<StorageUsageInfo> GetUsage() {
    base::RunLoop loop;
    std::vector<StorageUsageInfo> usage;
    partition()->GetDOMStorageContext()->GetLocalStorageUsage(
        base::BindLambdaForTesting([&](const std::vector<StorageUsageInfo>& u) {
          usage = u;
          loop.Quit();
        }));
    loop.Run();
    return usage;
  }

  void DeletePhysicalStorageKey(blink::StorageKey storage_key) {
    base::RunLoop loop;
    partition()->GetDOMStorageContext()->DeleteLocalStorage(storage_key,
                                                            loop.QuitClosure());
    loop.Run();
  }

  DOMStorageContextWrapper* context_wrapper() {
    return static_cast<DOMStorageContextWrapper*>(
        partition()->GetDOMStorageContext());
  }

  // Fuchsia's sandboxed SQLite VFS cannot acquire file locks, so a new SQLite
  // database cannot be opened there.
  // TODO(crbug.com/488731425): Re-enable the SQLite arm once the sandboxed
  // DomStorage SQLite backend runs on Fuchsia.
  bool IsSqliteBackendOnFuchsia() const {
#if BUILDFLAG(IS_FUCHSIA)
    return GetParam();
#else
    return false;
#endif
  }

  // Stores a >1 KB incompressible value in the current page's `storage_type`
  // ("localStorage" or "sessionStorage") so the database rounds to a non-zero
  // on-disk size on both backends.
  void WriteLargeValue(std::string_view storage_type) {
    ASSERT_TRUE(
        ExecJs(shell()->web_contents(),
               base::StrCat({"let value = '';"
                             "while (value.length < 16384) {"
                             "  value += Math.random().toString(36).slice(2);"
                             "}",
                             storage_type, ".setItem('big', value);"})));
  }

  void NavigateToTestOrigin() {
    ASSERT_TRUE(NavigateToURL(shell()->web_contents(),
                              GetTestUrl(nullptr, "empty.html")));
  }

  // `Flush` schedules a commit but returns without waiting for it. `GetUsage`
  // reads the database on the same sequence the commit runs on, so its reply
  // running ensures the flushed write has reached the disk.
  void CommitLocalStorage() {
    context_wrapper()->Flush();
    base::RunLoop loop;
    context_wrapper()->GetLocalStorageUsage(base::BindLambdaForTesting(
        [&](const std::vector<StorageUsageInfo>&) { loop.Quit(); }));
    loop.Run();
  }

  void CommitSessionStorage() {
    context_wrapper()->Flush();
    base::RunLoop loop;
    context_wrapper()->GetSessionStorageUsage(base::BindLambdaForTesting(
        [&](const std::vector<SessionStorageUsageInfo>&) { loop.Quit(); }));
    loop.Run();
  }

  // The size is recorded on a blocking task in the StorageService, so poll
  // until the sample reaches the browser process.
  void ExpectNonZeroOnDiskSize(const std::string& histogram) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      FetchHistogramsFromChildProcesses();
      return histograms_.GetTotalSum(histogram) > 0;
    }));
  }

 private:
  base::HistogramTester histograms_;
  base::test::ScopedFeatureList feature_list_;
};

static const bool kIncognito = true;
static const bool kNotIncognito = false;

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, SanityCheck) {
  SimpleTest(GetTestUrl("dom_storage", "sanity_check.html"), kNotIncognito);
}

// TODO(crbug.com/488417166): Fix flakiness on android-x86-rel and re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanityCheckIncognito DISABLED_SanityCheckIncognito
#else
#define MAYBE_SanityCheckIncognito SanityCheckIncognito
#endif
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, MAYBE_SanityCheckIncognito) {
  SimpleTest(GetTestUrl("dom_storage", "sanity_check.html"), kIncognito);
}

// http://crbug.com/654704 PRE_ tests aren't supported on Android.
// TODO(crbug.com/40885339): Re-enable this test for fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_DataPersists DISABLED_DataPersists
#else
#define MAYBE_DataPersists DataPersists
#endif
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, PRE_DataPersists) {
  SimpleTest(GetTestUrl("dom_storage", "store_data.html"), kNotIncognito);

  // Browser shutdown can always race with async work on non-shutdown-blocking
  // task runners. This includes the local storage implementation. If opening
  // the database takes too long, by the time it finishes the IO thread may be
  // shut down and the Local Storage implementation may be unable to commit its
  // pending operations.
  //
  // Since the point of this test is to verify that committed data is actually
  // retrievable by a subsequent browser session, wait for the database to be
  // ready.
  base::RunLoop loop;
  context_wrapper()->GetLocalStorageUsage(base::BindLambdaForTesting(
      [&](const std::vector<StorageUsageInfo>&) { loop.Quit(); }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, MAYBE_DataPersists) {
  SimpleTest(GetTestUrl("dom_storage", "verify_data.html"), kNotIncognito);
}

// A PRE_ run persists a >1 KB database. The main run reopens it and checks the
// on-disk size histogram records a non-zero sample on both backends.
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, PRE_LocalStorageOnDiskSize) {
  if (IsSqliteBackendOnFuchsia()) {
    GTEST_SKIP() << "SQLite DomStorage backend unsupported on Fuchsia";
  }
  NavigateToTestOrigin();
  WriteLargeValue("localStorage");
  CommitLocalStorage();
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, LocalStorageOnDiskSize) {
  if (IsSqliteBackendOnFuchsia()) {
    GTEST_SKIP() << "SQLite DomStorage backend unsupported on Fuchsia";
  }
  NavigateToTestOrigin();
  // Accessing localStorage opens its on-disk database, which records the size.
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "localStorage.length;"));
  ExpectNonZeroOnDiskSize("LocalStorage.DatabaseOnDiskSizeKB");
}

// SessionStorage on Android uses BackingMode::kClearDiskStateOnOpen, which
// destroys any pre-existing on-disk data on open. That makes the on-disk size
// unobservable across a restart, so we skip these tests on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, PRE_SessionStorageOnDiskSize) {
  if (IsSqliteBackendOnFuchsia()) {
    GTEST_SKIP() << "SQLite DomStorage backend unsupported on Fuchsia";
  }
  NavigateToTestOrigin();
  WriteLargeValue("sessionStorage");

  SessionStorageNamespace* session_storage_namespace =
      shell()
          ->web_contents()
          ->GetController()
          .GetDefaultSessionStorageNamespace();
  ASSERT_TRUE(session_storage_namespace);
  // SessionStorage only persists across a restart when its namespace opts in.
  session_storage_namespace->SetShouldPersist(true);

  CommitSessionStorage();
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, SessionStorageOnDiskSize) {
  if (IsSqliteBackendOnFuchsia()) {
    GTEST_SKIP() << "SQLite DomStorage backend unsupported on Fuchsia";
  }
  NavigateToTestOrigin();
  // Accessing sessionStorage opens its on-disk database, which records the
  // size.
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "sessionStorage.length;"));
  ExpectNonZeroOnDiskSize("Storage.SessionStorage.DatabaseOnDiskSizeKB");
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug/361107780): Fix flakiness on android-bfcache-rel and re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DeletePhysicalStorageKey DISABLED_DeletePhysicalStorageKey
#else
#define MAYBE_DeletePhysicalStorageKey DeletePhysicalStorageKey
#endif
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, MAYBE_DeletePhysicalStorageKey) {
  EXPECT_EQ(0U, GetUsage().size());
  SimpleTest(GetTestUrl("dom_storage", "store_data.html"), kNotIncognito);
  std::vector<StorageUsageInfo> usage = GetUsage();
  ASSERT_EQ(1U, usage.size());
  DeletePhysicalStorageKey(usage[0].storage_key);
  EXPECT_EQ(0U, GetUsage().size());
}

// On Windows file://localhost/C:/src/chromium/src/content/test/data/title1.html
// doesn't work.
#if !BUILDFLAG(IS_WIN)
// Regression test for https://crbug.com/776160.  The test verifies that there
// is no disagreement between 1) site URL used for browser-side isolation
// enforcement and 2) the origin requested by Blink.  Before this bug was fixed,
// (1) was file://localhost/ and (2) was file:// - this led to renderer kills.
IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest, FileUrlWithHost) {
  // Navigate to file://localhost/.../title1.html
  GURL regular_file_url = GetTestUrl(nullptr, "title1.html");
  GURL::Replacements host_replacement;
  host_replacement.SetHostStr("localhost");
  GURL file_with_host_url =
      regular_file_url.ReplaceComponents(host_replacement);
  EXPECT_TRUE(NavigateToURL(shell(), file_with_host_url));
  EXPECT_THAT(shell()->web_contents()->GetLastCommittedURL().spec(),
              testing::StartsWith("file://localhost/"));
  EXPECT_THAT(shell()->web_contents()->GetLastCommittedURL().spec(),
              testing::EndsWith("/title1.html"));

  // Verify that window.localStorage works fine.
  std::string script = R"(
      localStorage["foo"] = "bar";
      localStorage["foo"];
  )";
  EXPECT_EQ("bar", EvalJs(shell(), script));
}
#endif

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    DOMStorageBrowserTest,
    testing::Bool(),
    [](const testing::TestParamInfo<DOMStorageBrowserTest::ParamType>& info) {
      return info.param ? "SQLite" : "LevelDB";
    });

}  // namespace content
