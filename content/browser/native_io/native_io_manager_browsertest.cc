// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

class NativeIOManagerBrowserTest : public ContentBrowserTest {
 public:
  NativeIOManagerBrowserTest() {
    // SharedArrayBuffers are not enabled by default on Android, see also
    // https://crbug.com/1144104 .
    feature_list_.InitAndEnableFeature(features::kSharedArrayBuffer);
  }

  NativeIOManagerBrowserTest(const NativeIOManagerBrowserTest&) = delete;
  NativeIOManagerBrowserTest& operator=(const NativeIOManagerBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  base::FilePath GetNativeIODir(base::FilePath user_data_dir,
                                const GURL& test_url) {
    std::string origin_identifier =
        storage::GetIdentifierFromOrigin(test_url.DeprecatedGetOriginAsURL());
    base::FilePath root_dir =
        NativeIOManager::GetNativeIORootPath(user_data_dir);
    return root_dir.AppendASCII(origin_identifier);
  }

  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  static void DeleteNativeIODataOnIOThread(
      scoped_refptr<storage::QuotaManager> quota_manager,
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback) {
    quota_manager->FindAndDeleteBucketData(
        storage_key, storage::kDefaultBucketName, std::move(callback));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NativeIOManagerBrowserTest, ReadFromDeletedFile) {
  const GURL& test_url = embedded_test_server()->GetURL(
      "/native_io/read_from_deleted_file_test.html");
  Shell* browser = CreateBrowser();
  base::FilePath user_data_dir = GetNativeIODir(
      browser->web_contents()->GetBrowserContext()->GetPath(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, "writeToFile()").ExtractBool());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(user_data_dir.AppendASCII("test_file")));
  }
  EXPECT_TRUE(EvalJs(browser, "readFromFile()").ExtractBool());
}

// This test depends on POSIX file permissions, which do not work on Windows,
// Android, or Fuchsia.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(NativeIOManagerBrowserTest, TryOpenProtectedFileTest) {
  const GURL& test_url = embedded_test_server()->GetURL(
      "/native_io/try_open_protected_file_test.html");
  Shell* browser = CreateBrowser();
  base::FilePath user_data_dir_ = GetNativeIODir(
      browser->web_contents()->GetBrowserContext()->GetPath(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, "createAndCloseFile()").ExtractBool());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::SetPosixFilePermissions(user_data_dir_.AppendASCII("test_file"),
                                  0300);  // not readable
  }
  std::string expected_caught_error = "InvalidStateError";
  EXPECT_EQ(EvalJs(browser, "tryOpeningFile()").ExtractString(),
            expected_caught_error);
}
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_FUCHSIA)

// TODO(http://crbug.com/1177307): This test might be flaky on some Windows
// configurations.
#if BUILDFLAG(IS_WIN)
#define MAYBE_FileUsageAfterOriginRemoval DISABLED_FileUsageAfterOriginRemoval
#else
#define MAYBE_FileUsageAfterOriginRemoval FileUsageAfterOriginRemoval
#endif
IN_PROC_BROWSER_TEST_F(NativeIOManagerBrowserTest,
                       MAYBE_FileUsageAfterOriginRemoval) {
  const GURL& test_url = embedded_test_server()->GetURL(
      "/native_io/file_usage_after_origin_removal.html");
  Shell* browser = CreateBrowser();
  base::RunLoop run_loop;
  scoped_refptr<storage::QuotaManager> quota_manager =
      browser->web_contents()
          ->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetQuotaManager();

  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, "openFile()").ExtractBool());
  blink::mojom::QuotaStatusCode deletion_result;
  RunOnIOThreadBlocking(base::BindOnce(
      &NativeIOManagerBrowserTest::DeleteNativeIODataOnIOThread, quota_manager,
      blink::StorageKey::CreateFromStringForTesting(test_url.spec()),
      base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode result) {
        deletion_result = result;
        run_loop.Quit();
      })));
  run_loop.Run();

  EXPECT_EQ(deletion_result, blink::mojom::QuotaStatusCode::kOk);
  std::string expected_caught_error = "InvalidStateError";
  EXPECT_EQ(EvalJs(browser, "tryAccessOpenedFile()").ExtractString(),
            expected_caught_error);
  EXPECT_EQ(EvalJs(browser, "countFiles()").ExtractInt(), 0);
  EXPECT_TRUE(EvalJs(browser, "openAnotherFile()").ExtractBool());
}

IN_PROC_BROWSER_TEST_F(NativeIOManagerBrowserTest, ThrowsInIncognito) {
  const GURL& test_url =
      embedded_test_server()->GetURL("/native_io/throws_in_incognito.html");
  Shell* browser = CreateOffTheRecordBrowser();
  NavigateToURLBlockUntilNavigationsComplete(browser, test_url,
                                             /*number_of_navigations=*/1);
  EXPECT_TRUE(EvalJs(browser, "tryAccessStorageFoundation()").ExtractBool());
  EXPECT_TRUE(
      EvalJs(browser, "tryAccessStorageFoundationSync()").ExtractBool());
}

}  // namespace content
