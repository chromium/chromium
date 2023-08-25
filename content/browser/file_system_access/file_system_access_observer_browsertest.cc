// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/buildflag.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// TODO(https://crbug.com/1019297): Consider making these WPTs, and adding a
// lot more of them. For example:
//   - change types
//   - recursively watching a directory
//   - watching non-local file systems
//   - observing a handle without permission should fail
//   - changes should not be reported to swap files
//   - changes should not be reported if permission to the handle is lost
//   - changes should not be reported if the page is not fully-active
//   - moving an observed handle

class FileSystemAccessObserverBrowserTestBase : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(temp_dir_.Set(base::MakeLongFilePath(temp_dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)

    ASSERT_TRUE(embedded_test_server()->Start());
    test_url_ = embedded_test_server()->GetURL("/title1.html");

    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;
};

class FileSystemAccessObserverDefaultBrowserTest
    : public FileSystemAccessObserverBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverDefaultBrowserTest,
                       DisabledByDefault) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  auto result =
      EvalJs(shell(),
             "(async () => {"
             "function onChange(records, observer) {};"
             "const observer = new FileSystemObserver(onChange); })()");
  EXPECT_TRUE(result.error.find("not defined") != std::string::npos)
      << result.error;
}

class FileSystemAccessObserveWithFlagBrowserTest
    : public FileSystemAccessObserverBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the flag to use the FileSystemObserver interface.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "FileSystemObserver");
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserveWithFlagBrowserTest,
                       CreateObserver) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  EXPECT_TRUE(
      ExecJs(shell(),
             "(async () => {"
             "function onChange(records, observer) {};"
             "const observer = new FileSystemObserver(onChange); })()"));
}

class FileSystemAccessObserverBrowserTest
    : public FileSystemAccessObserverBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable read/write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, CreateObserver) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  EXPECT_TRUE(
      ExecJs(shell(),
             "(async () => {"
             "function onChange(records, observer) {};"
             "const observer = new FileSystemObserver(onChange); })()"));
}

// Local file system access - including the open*Picker() methods used here - is
// not supported on Android. See https://crbug.com/1011535.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveFile) {
  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, "observe me"));
  }

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. Instead, just check that attempting to observe a
// handle does not crash.
#if BUILDFLAG(IS_FUCHSIA)
  auto result = EvalJs(shell(),
                       "(async () => {"
                       "function onChange(records, observer) {};"
                       "const [file] = await self.showOpenFilePicker();"
                       "const observer = new FileSystemObserver(onChange);"
                       "await observer.observe(file); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
#else
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "let promiseResolve, promiseReject;"
                     "let promise = new Promise(function(resolve, reject) {"
                     "  promiseResolve = resolve;"
                     "  promiseReject = reject;"
                     "});"
                     "async function onChange(records, observer) {"
                     "  promiseResolve(true);"
                     "};"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(onChange);"
                     "await observer.observe(file);"
                     "const writable = await file.createWritable();"
                     "await writable.write('blah');"
                     "await writable.close();"
                     "return await promise; })()")
                  .ExtractBool());
#endif  // BUILDFLAG(IS_FUCHSIA)
}

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. This test would otherwise be the same as above, so
// just skip it.
#if !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveFileRename) {
  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, "observe me"));
  }

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{file_path}));
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "let promiseResolve, promiseReject;"
                     "let promise = new Promise(function(resolve, reject) {"
                     "  promiseResolve = resolve;"
                     "  promiseReject = reject;"
                     "});"
                     "async function onChange(records, observer) {"
                     "  promiseResolve(true);"
                     "};"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(onChange);"
                     "await observer.observe(file);"
                     "await file.move('newName.txt');"
                     "return await promise; })()")
                  .ExtractBool());
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveDirectory) {
  base::FilePath dir_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &dir_path));
  }

  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(
          std::vector<base::FilePath>{dir_path}));
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. Instead, just check that attempting to observe a
// handle does not crash.
#if BUILDFLAG(IS_FUCHSIA)
  auto result = EvalJs(shell(),
                       "(async () => {"
                       "function onChange(records, observer) {};"
                       "const dir = await self.showDirectoryPicker();"
                       "const observer = new FileSystemObserver(onChange);"
                       "await observer.observe(dir); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
#else
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "let promiseResolve, promiseReject;"
                     "let promise = new Promise(function(resolve, reject) {"
                     "  promiseResolve = resolve;"
                     "  promiseReject = reject;"
                     "});"
                     "async function onChange(records, observer) {"
                     "  promiseResolve(true);"
                     "};"
                     "const dir = await self.showDirectoryPicker();"
                     "const observer = new FileSystemObserver(onChange);"
                     "await observer.observe(dir);"
                     "await dir.getFileHandle('newFile.txt', { create:true });"
                     "return await promise; })()")
                  .ExtractBool());
#endif  // BUILDFLAG(IS_FUCHSIA)
}

#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveBucketFS) {
  // TODO(https://crbug.com/1019297): The BucketFS is not yet supported.

  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  auto result = EvalJs(shell(),
                       "(async () => {"
                       "function onChange(records, observer) {};"
                       "const root = await navigator.storage.getDirectory();"
                       "const observer = new FileSystemObserver(onChange);"
                       "await observer.observe(root); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
}

}  // namespace content
