// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace content {

constexpr const char kTestSessionStorageId[] =
    "574d2d70-24ca-4d8c-ae23-c7e1e39d07be";

// This browser test is aimed towards exercising the DOMStorage system
// from end-to-end.
class DOMStorageBrowserTest : public ContentBrowserTest {
 public:
  DOMStorageBrowserTest() {}

  void SimpleTest(const GURL& test_url, bool incognito) {
    // The test page will perform tests then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

  std::vector<LocalStorageUsageInfo> GetUsage() {
    auto* context = BrowserContext::GetDefaultStoragePartition(
                        shell()->web_contents()->GetBrowserContext())
                        ->GetDOMStorageContext();
    base::RunLoop loop;
    std::vector<LocalStorageUsageInfo> usage;
    context->GetLocalStorageUsage(base::BindLambdaForTesting(
        [&](const std::vector<LocalStorageUsageInfo>& u) {
          usage = u;
          loop.Quit();
        }));
    loop.Run();
    return usage;
  }

  void DeletePhysicalOrigin(GURL origin) {
    auto* context = BrowserContext::GetDefaultStoragePartition(
                        shell()->web_contents()->GetBrowserContext())
                        ->GetDOMStorageContext();
    base::RunLoop loop;
    context->DeleteLocalStorage(origin, loop.QuitClosure());
    loop.Run();
  }

  DOMStorageContextWrapper* context_wrapper() {
    return static_cast<DOMStorageContextWrapper*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext())
            ->GetDOMStorageContext());
  }

  base::SequencedTaskRunner* mojo_task_runner() {
    return context_wrapper()->mojo_task_runner();
  }

  LocalStorageContextMojo* context() { return context_wrapper()->mojo_state_; }

  SessionStorageContextMojo* session_storage_context() {
    return context_wrapper()->mojo_session_state_;
  }

  base::FilePath legacy_localstorage_path() {
    return context()->old_localstorage_path_;
  }

  void EnsureConnected() {
    base::RunLoop run_loop;
    mojo_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalStorageContextMojo::RunWhenConnected,
                       base::Unretained(context()), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void EnsureSessionStorageConnected() {
    base::RunLoop run_loop;
    mojo_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&SessionStorageContextMojo::RunWhenConnected,
                                  base::Unretained(session_storage_context()),
                                  run_loop.QuitClosure()));
    run_loop.Run();
  }
};

static const bool kIncognito = true;
static const bool kNotIncognito = false;

IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, SanityCheck) {
  SimpleTest(GetTestUrl("dom_storage", "sanity_check.html"), kNotIncognito);
}

IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, SanityCheckIncognito) {
  SimpleTest(GetTestUrl("dom_storage", "sanity_check.html"), kIncognito);
}

// http://crbug.com/654704 PRE_ tests aren't supported on Android.
#if defined(OS_ANDROID)
#define MAYBE_DataPersists DISABLED_DataPersists
#else
#define MAYBE_DataPersists DataPersists
#endif
IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, PRE_DataPersists) {
  EnsureConnected();
  SimpleTest(GetTestUrl("dom_storage", "store_data.html"), kNotIncognito);
}

IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, MAYBE_DataPersists) {
  SimpleTest(GetTestUrl("dom_storage", "verify_data.html"), kNotIncognito);
}

IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, DeletePhysicalOrigin) {
  EXPECT_EQ(0U, GetUsage().size());
  SimpleTest(GetTestUrl("dom_storage", "store_data.html"), kNotIncognito);
  std::vector<LocalStorageUsageInfo> usage = GetUsage();
  ASSERT_EQ(1U, usage.size());
  DeletePhysicalOrigin(usage[0].origin);
  EXPECT_EQ(0U, GetUsage().size());
}

// On Windows file://localhost/C:/src/chromium/src/content/test/data/title1.html
// doesn't work.
#if !defined(OS_WIN)
// Regression test for https://crbug.com/776160.  The test verifies that there
// is no disagreement between 1) site URL used for browser-side isolation
// enforcement and 2) the origin requested by Blink.  Before this bug was fixed,
// (1) was file://localhost/ and (2) was file:// - this led to renderer kills.
IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, FileUrlWithHost) {
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
  std::string result;
  std::string script = R"(
      localStorage["foo"] = "bar";
      domAutomationController.send(localStorage["foo"]);
  )";
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &result));
  EXPECT_EQ("bar", result);
}
#endif

IN_PROC_BROWSER_TEST_F(DOMStorageBrowserTest, DataMigrates) {
  base::FilePath db_path = legacy_localstorage_path().Append(
      DOMStorageArea::DatabaseFileNameFromOrigin(
          url::Origin::Create(GetTestUrl("dom_storage", "store_data.html"))));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(legacy_localstorage_path()));
    DOMStorageDatabase db(db_path);
    DOMStorageValuesMap data;
    data[base::ASCIIToUTF16("foo")] =
        base::NullableString16(base::ASCIIToUTF16("bar"), false);
    db.CommitChanges(false, data);
    EXPECT_TRUE(base::PathExists(db_path));
  }
  std::vector<LocalStorageUsageInfo> usage = GetUsage();
  ASSERT_EQ(1U, usage.size());
  EXPECT_GT(usage[0].data_size, 6u);

  SimpleTest(GetTestUrl("dom_storage", "verify_data.html"), kNotIncognito);
  usage = GetUsage();
  ASSERT_EQ(1U, usage.size());
  EXPECT_GT(usage[0].data_size, 6u);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_FALSE(base::PathExists(db_path));
  }
}

class DOMStorageMigrationBrowserTest : public DOMStorageBrowserTest {
 public:
  DOMStorageMigrationBrowserTest() : DOMStorageBrowserTest() {
    if (IsPreTest())
      feature_list_.InitAndDisableFeature(
          blink::features::kOnionSoupDOMStorage);
    else
      feature_list_.InitAndEnableFeature(blink::features::kOnionSoupDOMStorage);
  }

  void SessionStorageTest(const GURL& test_url) {
    // The test page will perform tests then navigate to either
    // a #pass or #fail ref.
    context_wrapper()->SetSaveSessionStorageOnDisk();
    scoped_refptr<SessionStorageNamespaceImpl> ss_namespace =
        SessionStorageNamespaceImpl::Create(context_wrapper(),
                                            kTestSessionStorageId);
    ss_namespace->SetShouldPersist(true);
    Shell* the_browser = Shell::CreateNewWindowWithSessionStorageNamespace(
        ShellContentBrowserClient::Get()->browser_context(),
        GURL(url::kAboutBlankURL), nullptr, gfx::Size(),
        std::move(ss_namespace));
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    context_wrapper()->Flush();
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DOMStorageMigrationBrowserTest, PRE_DataMigrates) {
  SessionStorageTest(
      GetTestUrl("dom_storage", "store_session_storage_data.html"));
}

// http://crbug.com/654704 PRE_ tests aren't supported on Android.
#if defined(OS_ANDROID)
#define MAYBE_DataMigrates DISABLED_DataMigrates
#else
#define MAYBE_DataMigrates DataMigrates
#endif
IN_PROC_BROWSER_TEST_F(DOMStorageMigrationBrowserTest, MAYBE_DataMigrates) {
  EXPECT_TRUE(session_storage_context());
  EnsureSessionStorageConnected();
  SessionStorageTest(
      GetTestUrl("dom_storage", "verify_session_storage_data.html"));

  // Check that we migrated from v0 (no version) to v1.
  base::RunLoop loop;
  leveldb::mojom::LevelDBDatabase* database =
      session_storage_context()->DatabaseForTesting();
  mojo_task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        database->Get(
            leveldb::StringPieceToUint8Vector("version"),
            base::BindLambdaForTesting([&](leveldb::mojom::DatabaseError error,
                                           const std::vector<uint8_t>& value) {
              EXPECT_EQ(leveldb::mojom::DatabaseError::OK, error);
              EXPECT_EQ(base::StringPiece("1"),
                        leveldb::Uint8VectorToStringPiece(value));
              loop.Quit();
            }));
      }));
  loop.Run();
}

}  // namespace content
