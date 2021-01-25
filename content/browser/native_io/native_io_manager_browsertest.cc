// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/common/database/database_identifier.h"
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
        storage::GetIdentifierFromOrigin(test_url.GetOrigin());
    base::FilePath root_dir =
        NativeIOManager::GetNativeIORootPath(user_data_dir);
    return root_dir.AppendASCII(origin_identifier);
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
#if !defined(OS_WIN) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
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
#endif  // !defined(OS_WIN) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA)

}  // namespace content
