// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/values_util.h"
#include "base/test/test_timeouts.h"
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

  base::FilePath CreateFileToBePicked() {
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
    return file_path;
  }

  base::FilePath CreateDirectoryToBePicked() {
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
    return dir_path;
  }

  bool CreatePromiseAndResolvers() {
    return ExecJs(shell(),
                  "(async () => {"
                  "self.promise = new Promise(function(resolve, reject) {"
                  "  self.promiseResolve = resolve;"
                  "  self.promiseReject = reject;"
                  "});"
                  "async function onChange(records, observer) {"
                  "  self.promiseResolve(true);"
                  "};"
                  "self.onChange = onChange; })()");
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
             "const observer = new FileSystemObserver(() => {}); })()");
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
             "const observer = new FileSystemObserver(() => {}); })()"));
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
             "const observer = new FileSystemObserver(() => {}); })()"));
}

// Local file system access - including the open*Picker() methods used here - is
// not supported on Android. See https://crbug.com/1011535.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveFile) {
  base::FilePath file_path = CreateFileToBePicked();

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. Instead, just check that attempting to observe a
// handle does not crash.
#if BUILDFLAG(IS_FUCHSIA)
  auto result = EvalJs(shell(),
                       "(async () => {"
                       "const [file] = await self.showOpenFilePicker();"
                       "const observer = new FileSystemObserver(() => {});"
                       "await observer.observe(file); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
#else
  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(self.onChange);"
                     "await observer.observe(file);"
                     "const writable = await file.createWritable();"
                     "await writable.write('blah');"
                     "await writable.close();"
                     "return await self.promise; })()")
                  .ExtractBool());
#endif  // BUILDFLAG(IS_FUCHSIA)
}

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. This test would otherwise be the same as above, so
// just skip it.
#if !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveFileRename) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(self.onChange);"
                     "await observer.observe(file);"
                     "await file.move('newName.txt');"
                     "return await self.promise; })()")
                  .ExtractBool());
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveDirectory) {
  base::FilePath dir_path = CreateDirectoryToBePicked();

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. Instead, just check that attempting to observe a
// handle does not crash.
#if BUILDFLAG(IS_FUCHSIA)
  auto result = EvalJs(shell(),
                       "(async () => {"
                       "const dir = await self.showDirectoryPicker();"
                       "const observer = new FileSystemObserver(() => {});"
                       "await observer.observe(dir); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
#else
  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "const dir = await self.showDirectoryPicker();"
                     "const observer = new FileSystemObserver(self.onChange);"
                     "await observer.observe(dir);"
                     "await dir.getFileHandle('newFile.txt', { create:true });"
                     "return await self.promise; })()")
                  .ExtractBool());
#endif  // BUILDFLAG(IS_FUCHSIA)
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveDirectoryRecursively) {
  base::FilePath dir_path = CreateDirectoryToBePicked();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::CreateDirectory(dir_path.AppendASCII("sub1"));
    base::CreateDirectory(dir_path.AppendASCII("sub1").AppendASCII("sub2"));
  }

// `base::FilePatchWatcher` is not implemented on Fuchsia. See
// https://crbug.com/851641. Instead, just check that attempting to observe a
// handle does not crash.
// Meanwhile, recursive watches are not supported on iOS.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
  auto result =
      EvalJs(shell(),
             "(async () => {"
             "const dir = await self.showDirectoryPicker();"
             "const observer = new FileSystemObserver(() => {});"
             "await observer.observe(dir, { recursive: true }); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
#else
  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(
      EvalJs(shell(),
             "(async () => {"
             "const dir = await self.showDirectoryPicker();"
             "const observer = new FileSystemObserver(self.onChange);"
             "await observer.observe(dir, { recursive: true });"
             "const subDir1 = await dir.getDirectoryHandle('sub1');"
             "const subDir2 = await subDir1.getDirectoryHandle('sub2');"
             "await subDir2.getFileHandle('newFile.txt', { create: true });"
             "return await self.promise; })()")
          .ExtractBool());
#endif  // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
}

#if !BUILDFLAG(IS_FUCHSIA)

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveThenUnobserve) {
  base::FilePath file_path = CreateFileToBePicked();

  // Calling unobserve() with a corresponding observe() should not crash.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(() => {});"
                     "await observer.observe(file);"
                     "observer.unobserve(file); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveThenUnobserveUnrelated) {
  base::FilePath file_path = CreateFileToBePicked();

  // Calling unobserve() with a handle unrelated to a corresponding observe()
  // should not crash.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const root = await navigator.storage.getDirectory();"
                     "const observer = new FileSystemObserver(() => {});"
                     "await observer.observe(file);"
                     "observer.unobserve(root); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       NoChangesAfterUnobserve) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // No changes should be received. The promise should be resolved after the
  // setTimeout().
  EXPECT_FALSE(
      EvalJs(shell(),
             JsReplace(
                 "(async () => {"
                 "const [file] = await self.showOpenFilePicker();"
                 "const observer = new FileSystemObserver(self.onChange);"
                 "await observer.observe(file);"
                 "observer.unobserve(file);"
                 "const writable = await file.createWritable();"
                 "await writable.write('blah');"
                 "await writable.close();"
                 "setTimeout(() => {"
                 "  self.promiseResolve(false);"  // No changes received. Good!
                 "}, $1);"
                 "return await self.promise; })()",
                 base::Int64ToValue(
                     TestTimeouts::tiny_timeout().InMilliseconds())))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveThenDisconnect) {
  base::FilePath file_path = CreateFileToBePicked();

  // Calling disconnect() after calling observe() should not crash.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(() => {});"
                     "await observer.observe(file);"
                     "observer.disconnect(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       NoChangesAfterDisconnect) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // No changes should be received. The promise should be resolved after the
  // setTimeout().
  EXPECT_FALSE(
      EvalJs(shell(),
             JsReplace(
                 "(async () => {"
                 "const [file] = await self.showOpenFilePicker();"
                 "const observer = new FileSystemObserver(self.onChange);"
                 "await observer.observe(file);"
                 "observer.disconnect();"
                 "const writable = await file.createWritable();"
                 "await writable.write('blah');"
                 "await writable.close();"
                 "setTimeout(() => {"
                 "  self.promiseResolve(false);"  // No changes received. Good!
                 "}, $1);"
                 "return await self.promise; })()",
                 base::Int64ToValue(
                     TestTimeouts::tiny_timeout().InMilliseconds())))
          .ExtractBool());
}

// TODO(https://crbug.com/1019297): Add a ReObserveAfterUnobserve test once the
// unobserve() method is no longer racy. See https://crrev.com/c/4814709.
IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ReObserveAfterDisconnect) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(self.onChange);"
                     "await observer.observe(file);"
                     "observer.disconnect();"
                     "await observer.observe(file);"  // Start observing the
                                                      // file again.
                     "const writable = await file.createWritable();"
                     "await writable.write('blah');"
                     "await writable.close();"  // We should see this change.
                     "return await self.promise; })()")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveFileReportsModifiedType) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // TODO(https://crbug.com/1425601): Support change types. For now, just
  // confirm that "modified" is plumbed through properly.
  EXPECT_TRUE(EvalJs(shell(),
                     "(async () => {"
                     "async function onChange(records, observer) {"
                     "  const record = records[0];"
                     "  promiseResolve(record.type === 'modified');"
                     "};"
                     "const [file] = await self.showOpenFilePicker();"
                     "const observer = new FileSystemObserver(onChange);"
                     "await observer.observe(file);"
                     "const writable = await file.createWritable();"
                     "await writable.write('blah');"
                     "await writable.close();"
                     "return await self.promise; })()")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveFileReportsCorrectHandle) {
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

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // The `changedHandle` should the same as `root`, which is the same as the
  // handle passed to `observe()`.
  EXPECT_TRUE(
      EvalJs(shell(),
             "(async () => {"
             "async function onChange(records, observer) {"
             "  const record = records[0];"
             "  promiseResolve(await file.isSameEntry(record.root) &&"
             "                 await file.isSameEntry(record.changedHandle));"
             "};"
             "const [file] = await self.showOpenFilePicker();"
             "const observer = new FileSystemObserver(onChange);"
             "await observer.observe(file);"
             "const writable = await file.createWritable();"
             "await writable.write('blah');"
             "await writable.close();"
             "return await self.promise; })()")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveFileReportsCorrectRelativePathComponents) {
  base::FilePath file_path = CreateFileToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // The `relativePathComponents` should be an empty array, since the change
  // occurred on the path corresponding to the handle passed to `observe()`.
  EXPECT_TRUE(
      EvalJs(shell(),
             "(async () => {"
             "async function onChange(records, observer) {"
             "  const record = records[0];"
             "  promiseResolve(record.relativePathComponents.length === 0);"
             "};"
             "const [file] = await self.showOpenFilePicker();"
             "const observer = new FileSystemObserver(onChange);"
             "await observer.observe(file);"
             "const writable = await file.createWritable();"
             "await writable.write('blah');"
             "await writable.close();"
             "return await self.promise; })()")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveDirectoryReportsCorrectHandle) {
  base::FilePath dir_path = CreateDirectoryToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  // TODO(https://crbug.com/1425601): Don't assume the type of the changed
  // handle is the same as the type of the handle passed into observe().
  EXPECT_TRUE(
      EvalJs(shell(),
             "(async () => {"
             "async function onChange(records, observer) {"
             "  const record = records[0];"
             "  promiseResolve(await dir.isSameEntry(record.root) &&"
  // TODO(https://crbug.com/1425601): Some platforms do not report the modified
  // path. In these cases, `changedHandle` will always be the same as `root`.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
             "                 await subDir.isSameEntry(record.changedHandle));"
#else
             "                 await dir.isSameEntry(record.changedHandle));"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
             "};"
             "const dir = await self.showDirectoryPicker();"
             "const observer = new FileSystemObserver(onChange);"
             "await observer.observe(dir);"
             "const subDir = await dir.getDirectoryHandle('subdir', { "
             "create:true });"
             "return await self.promise; })()")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       ObserveDirectoryReportsCorrectRelativePathComponents) {
  base::FilePath dir_path = CreateDirectoryToBePicked();

  ASSERT_TRUE(CreatePromiseAndResolvers());
  EXPECT_TRUE(
      EvalJs(shell(),
             "(async () => {"
             "async function onChange(records, observer) {"
             "  const record = records[0];"
  // TODO(https://crbug.com/1425601): Some platforms do not report the modified
  // path. In these cases, `relativePathComponents` will always be empty.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
             "  promiseResolve(record.relativePathComponents.length === 1);"
#else
             "  promiseResolve(record.relativePathComponents.length === 0);"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
             "};"
             "const dir = await self.showDirectoryPicker();"
             "const observer = new FileSystemObserver(onChange);"
             "await observer.observe(dir);"
             "const subDir = await dir.getDirectoryHandle('subdir', { "
             "create:true });"
             "return await self.promise; })()")
          .ExtractBool());
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest, ObserveBucketFS) {
  // TODO(https://crbug.com/1019297): The BucketFS is not yet supported.

  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  auto result = EvalJs(shell(),
                       "(async () => {"
                       "const root = await navigator.storage.getDirectory();"
                       "const observer = new FileSystemObserver(() => {});"
                       "await observer.observe(root); })()");
  EXPECT_TRUE(result.error.find("did not support") != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       NothingToUnobserve) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  // Calling unobserve() without a corresponding observe() should be a no-op.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const observer = new FileSystemObserver(() => {});"
                     "const root = await navigator.storage.getDirectory();"
                     "observer.unobserve(root); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       NothingToDisconnect) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  // Calling disconnect() multiple times should be a no-op.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const observer = new FileSystemObserver(() => {});"
                     "observer.disconnect();"
                     "observer.disconnect(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       UnobserveIsIdempotent) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  // unobserve() may be called several times without crashing.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const observer = new FileSystemObserver(() => {});"
                     "const root = await navigator.storage.getDirectory();"
                     "observer.unobserve(root);"
                     "observer.unobserve(root);"
                     "observer.unobserve(root); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessObserverBrowserTest,
                       DisconnectIsIdempotent) {
  EXPECT_TRUE(NavigateToURL(shell(), test_url_));

  // disconnect() may be called several times without crashing.
  EXPECT_TRUE(ExecJs(shell(),
                     "(async () => {"
                     "const observer = new FileSystemObserver(() => {});"
                     "observer.disconnect();"
                     "observer.disconnect();"
                     "observer.disconnect(); })()"));
}

}  // namespace content
