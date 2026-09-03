// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/public/cpp/constants.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace_handle.h"
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
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

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

  std::string CreateSessionStorageNamespace() {
    std::string namespace_id =
        base::Uuid::GenerateRandomV4().AsLowercaseString();
    context_wrapper()->GetSessionStorageControl()->CreateNamespace(
        namespace_id);
    return namespace_id;
  }

  mojo::Remote<blink::mojom::StorageArea> BindSessionStorageArea(
      const blink::StorageKey& storage_key,
      const std::string& namespace_id) {
    mojo::Remote<blink::mojom::StorageArea> area;
    context_wrapper()->GetSessionStorageControl()->BindStorageArea(
        storage_key, namespace_id, area.BindNewPipeAndPassReceiver());
    return area;
  }

  void PutSessionStorageValue(mojo::Remote<blink::mojom::StorageArea>& area,
                              std::string_view value) {
    base::test::TestFuture<bool> future;
    area->Put(/*key=*/{'k', 'e', 'y'},
              /*value=*/std::vector<uint8_t>(value.begin(), value.end()),
              /*client_old_value=*/std::nullopt,
              /*source=*/nullptr, future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

  std::optional<std::string> GetSessionStorageValue(
      mojo::Remote<blink::mojom::StorageArea>& area) {
    base::test::TestFuture<std::vector<blink::mojom::KeyValuePtr>> future;
    area->GetAll(/*new_observer=*/mojo::NullRemote(), future.GetCallback());
    for (const auto& key_value : future.Take()) {
      if (key_value->key == std::vector<uint8_t>({'k', 'e', 'y'})) {
        return std::string(key_value->value.begin(), key_value->value.end());
      }
    }
    return std::nullopt;
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

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest,
                       ClearDataForStorageKeyClearsSessionStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL target_url =
      embedded_test_server()->GetURL("localhost", "/empty.html");
  const GURL other_url =
      embedded_test_server()->GetURL("127.0.0.1", "/empty.html");

  ASSERT_TRUE(NavigateToURL(shell(), target_url));
  ASSERT_TRUE(ExecJs(shell(), R"(
    localStorage.setItem('key', 'target-local');
    sessionStorage.setItem('key', 'target-session');
  )"));

  ASSERT_TRUE(NavigateToURL(shell(), other_url));
  ASSERT_TRUE(ExecJs(shell(), R"(
    localStorage.setItem('key', 'other-local');
    sessionStorage.setItem('key', 'other-session');
  )"));
  CommitSessionStorage();

  base::RunLoop loop;
  partition()->ClearData(
      StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(target_url)),
      base::Time(), base::Time::Max(), loop.QuitClosure());
  loop.Run();

  EXPECT_EQ("other-local",
            EvalJs(shell(), "localStorage.getItem('key')").ExtractString());
  EXPECT_EQ("other-session",
            EvalJs(shell(), "sessionStorage.getItem('key')").ExtractString());

  ASSERT_TRUE(NavigateToURL(shell(), target_url));
  EXPECT_TRUE(
      EvalJs(shell(), "localStorage.getItem('key') === null").ExtractBool());
  EXPECT_TRUE(
      EvalJs(shell(), "sessionStorage.getItem('key') === null").ExtractBool());
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest,
                       ClearDataForStorageKeyUsesFullStorageKey) {
  const url::Origin shared_origin =
      url::Origin::Create(GURL("https://resource.test"));
  const blink::StorageKey target_key = blink::StorageKey::Create(
      shared_origin, net::SchemefulSite(GURL("https://top-a.test")),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);
  const blink::StorageKey sibling_key = blink::StorageKey::Create(
      shared_origin, net::SchemefulSite(GURL("https://top-b.test")),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);

  const std::string namespace_a = CreateSessionStorageNamespace();
  const std::string namespace_b = CreateSessionStorageNamespace();
  auto target_area_a = BindSessionStorageArea(target_key, namespace_a);
  auto target_area_b = BindSessionStorageArea(target_key, namespace_b);
  auto sibling_area_a = BindSessionStorageArea(sibling_key, namespace_a);
  auto sibling_area_b = BindSessionStorageArea(sibling_key, namespace_b);
  PutSessionStorageValue(target_area_a, "target-a");
  PutSessionStorageValue(target_area_b, "target-b");
  PutSessionStorageValue(sibling_area_a, "sibling-a");
  PutSessionStorageValue(sibling_area_b, "sibling-b");

  base::RunLoop loop;
  partition()->ClearData(StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE,
                         target_key, base::Time(), base::Time::Max(),
                         loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(std::nullopt, GetSessionStorageValue(target_area_a));
  EXPECT_EQ(std::nullopt, GetSessionStorageValue(target_area_b));
  EXPECT_EQ("sibling-a", GetSessionStorageValue(sibling_area_a));
  EXPECT_EQ("sibling-b", GetSessionStorageValue(sibling_area_b));
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest,
                       ClearDataWithNullMatcherClearsAllSessionStorage) {
  const std::string namespace_id = CreateSessionStorageNamespace();
  auto area_a =
      BindSessionStorageArea(blink::StorageKey::CreateFirstParty(
                                 url::Origin::Create(GURL("https://a.test"))),
                             namespace_id);
  auto area_b =
      BindSessionStorageArea(blink::StorageKey::CreateFirstParty(
                                 url::Origin::Create(GURL("https://b.test"))),
                             namespace_id);
  PutSessionStorageValue(area_a, "a");
  PutSessionStorageValue(area_b, "b");

  base::RunLoop loop;
  partition()->ClearData(StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE,
                         blink::StorageKey(), base::Time(), base::Time::Max(),
                         loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(std::nullopt, GetSessionStorageValue(area_a));
  EXPECT_EQ(std::nullopt, GetSessionStorageValue(area_b));
}

IN_PROC_BROWSER_TEST_P(DOMStorageBrowserTest,
                       ClearDataCombinesSessionStorageFilterAndPolicy) {
  const url::Origin filtered_origin =
      url::Origin::Create(GURL("https://filtered.test"));
  const blink::StorageKey target_key = blink::StorageKey::Create(
      filtered_origin, net::SchemefulSite(GURL("https://top-a.test")),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);
  const blink::StorageKey policy_rejected_key = blink::StorageKey::Create(
      filtered_origin, net::SchemefulSite(GURL("https://top-b.test")),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);
  const blink::StorageKey filter_rejected_key = blink::StorageKey::Create(
      url::Origin::Create(GURL("https://unfiltered.test")),
      net::SchemefulSite(GURL("https://top-a.test")),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);

  const std::string namespace_id = CreateSessionStorageNamespace();
  auto target_area = BindSessionStorageArea(target_key, namespace_id);
  auto policy_rejected_area =
      BindSessionStorageArea(policy_rejected_key, namespace_id);
  auto filter_rejected_area =
      BindSessionStorageArea(filter_rejected_key, namespace_id);
  PutSessionStorageValue(target_area, "target");
  PutSessionStorageValue(policy_rejected_area, "policy-rejected");
  PutSessionStorageValue(filter_rejected_area, "filter-rejected");

  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete,
      BrowsingDataFilterBuilder::OriginMatchingMode::kOriginInAllContexts);
  filter_builder->AddOrigin(filtered_origin);
  auto policy_matcher = base::BindLambdaForTesting(
      [target_key, filter_rejected_key](const blink::StorageKey& storage_key,
                                        storage::SpecialStoragePolicy*) {
        return storage_key == target_key || storage_key == filter_rejected_key;
      });

  base::RunLoop loop;
  partition()->ClearData(StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE,
                         filter_builder.get(), std::move(policy_matcher),
                         /*cookie_deletion_filter=*/nullptr,
                         /*perform_storage_cleanup=*/false, base::Time(),
                         base::Time::Max(), loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(std::nullopt, GetSessionStorageValue(target_area));
  EXPECT_EQ("policy-rejected", GetSessionStorageValue(policy_rejected_area));
  EXPECT_EQ("filter-rejected", GetSessionStorageValue(filter_rejected_area));
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

  SessionStorageNamespaceHandle* session_storage_namespace =
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
