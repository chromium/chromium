// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace content {

// End-to-end tests for File System Access via clipboard, or more specifically,
// DataTransferItem::getAsFileSystemHandle().

class FileSystemAccessClipboardBrowserTest : public ContentBrowserTest {
 public:
  FileSystemAccessClipboardBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        blink::features::kFileSystemAccessLocal);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
    ui::TestClipboard::CreateForCurrentThread();
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath CreateTestFileInDirectory(const base::FilePath& directory_path,
                                           const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(directory_path, &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  base::FilePath CreateTestDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &result));
    return result;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessClipboardBrowserTest, File) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Create a promise that will resolve on paste with the text from the file on
  // the clipboard. The text is retrieved using the FileSystemAccess
  // getAsFileSystemHandle() function. The promise will reject if zero/multiple
  // items are on the clipboard or if the item is not a file.
  ASSERT_TRUE(
      ExecJs(shell(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.files.length !== 1) {"
             "      reject('There were ' + event.clipboardData.files.length +"
             "             ' clipboard files. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'file') {"
             "      reject('The clipboard item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             'file.');"
             "    }"
             "    const fileItem = event.clipboardData.items[0];"
             "    const fileHandle = await fileItem.getAsFileSystemHandle();"
             "    if (fileHandle.kind !== 'file') {"
             "       reject('The clipboard item was a directory, expected ' +"
             "              ' it to be a file.');"
             "    }"
             "    const file = await fileHandle.getFile();"
             "    const text = await file.text();"
             "    resolve(text);"
             "  };"
             "});"));

  // Create a test file with contents `test_contents` to put on clipboard.
  std::string test_contents = "Deleted code is debugged code.";
  base::FilePath test_file_path =
      CreateTestFileInDirectory(temp_dir_.GetPath(), test_contents);
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames(ui::FileInfosToURIList(
        {ui::FileInfo(test_file_path, base::FilePath())}));
  }

  // Send paste event and wait for JS promise to resolve with `test_contents`.
  shell()->web_contents()->Paste();
  EXPECT_EQ(test_contents, EvalJs(shell(), "p"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessClipboardBrowserTest, Directory) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Create a promise that will resolve on paste with the name of the child
  // directory of the clipboard directory. The promise rejects if there are
  // zero/multiple items on the clipboard or if the item is not a directory with
  // a single child.
  ASSERT_TRUE(
      ExecJs(shell(),
             "var p = new Promise((resolve, reject) => {"
             "  window.document.onpaste = async (event) => {"
             "    if (event.clipboardData.items.length !== 1) {"
             "      reject('There were ' + event.clipboardData.items.length +"
             "             ' clipboard items. Expected 1.');"
             "    }"
             "    if (event.clipboardData.items[0].kind != 'file') {"
             "      reject('The drag item was of kind: ' +"
             "             event.clipboardData.items[0].kind + '. Expected ' +"
             "             ' file.');"
             "    }"
             "    const fileItem = event.clipboardData.items[0];"
             "    directoryHandle = await fileItem.getAsFileSystemHandle();"
             "    if (directoryHandle.kind !== 'directory') {"
             "      reject('The dragged item was a file, expected it to be ' +"
             "             'a directory.');"
             "    }"
             "    let directoryContents = [];"
             "    for await (let fileName of directoryHandle.keys()) {"
             "      directoryContents.push(fileName);"
             "    };"
             "    if (directoryContents.length !== 1) {"
             "      reject('There were ', directoryContents.length,"
             "             ' files in the clipboard directory. Expected 1.');"
             "    }"
             "    resolve(directoryContents.pop());"
             "  };"
             "});"));

  // Create a directory with a file inside and place on the clipboard.
  const base::FilePath test_dir_path = CreateTestDir();
  std::string contents = "Irrelevant contents.";
  const base::FilePath file_inside_dir =
      CreateTestFileInDirectory(test_dir_path, contents);
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames(ui::FileInfosToURIList(
        {ui::FileInfo(test_dir_path, base::FilePath())}));
  }

  // Send paste event and wait for promise to resolve and expect the directory
  // to have child with name matching the base name of `file_inside_dir`.
  shell()->web_contents()->Paste();
  EXPECT_EQ(file_inside_dir.BaseName().AsUTF8Unsafe(), EvalJs(shell(), "p"));
}

}  // namespace content
