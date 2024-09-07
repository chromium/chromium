// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {

// End-to-end tests for File System Access drag and drop, or more specifically,
// DataTransferItem's getAsFileSystemHandle method.

class FileSystemAccessDragDropBrowserTest : public ContentBrowserTest {
 public:
  FileSystemAccessDragDropBrowserTest() {
    scoped_features_.InitAndEnableFeature(
        blink::features::kFileSystemAccessLocal);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
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

  RenderWidgetHostImpl* GetRenderWidgetHostImplForMainFrame() {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessDragDropBrowserTest, DropFile) {
  // Get the RenderWidgetHostImpl for the main (and only) frame.
  RenderWidgetHostImpl* render_widget_host_impl =
      GetRenderWidgetHostImplForMainFrame();
  DCHECK(render_widget_host_impl);

  // Prepare the window for dragging and dropping.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Prevent defaults of drag operations and create a promise that will resolve
  // on call to window.ondrop with the text from a dropped file. This text is
  // retrieved using the FileSystemAccess getAsFileSystemHandle function. The
  // promise will reject if zero/multiples items are dropped or if the item is
  // not a file.
  ASSERT_TRUE(
      ExecJs(shell(),
             "window.ondragenter = (e) => { e.preventDefault() };"
             "window.ondragover = (e) => { e.preventDefault() };"
             "var p = new Promise((resolve, reject) => {"
             "  window.ondrop = async (event) => {"
             "    event.preventDefault();"
             "    if (event.dataTransfer.items.length !== 1) {"
             "      reject('There were ' + event.dataTransfer.items.length"
             "              + ' dropped items. Expected 1.');"
             "    }"
             "    if (event.dataTransfer.items[0].kind != 'file') {"
             "      reject('The drag item was of kind: ' +"
             "              event.dataTransfer.items[0].kind + '. Expected"
             "              file.');"
             "    }"
             "    const fileItem = event.dataTransfer.items[0];"
             "    const fileHandle = await fileItem.getAsFileSystemHandle();"
             "    if (fileHandle.kind !== 'file') {"
             "       reject('The dragged item was a directory, expected it to"
             "               be a file.');"
             "    }"
             "    const file = await fileHandle.getFile();"
             "    const text = await file.text();"
             "    resolve(text);"
             "  }"
             "});"));

  // Create a test file with contents `test_contents` to drop on the webpage.
  std::string test_contents = "Deleted code is debugged code.";
  base::FilePath test_file_path =
      CreateTestFileInDirectory(temp_dir_.GetPath(), test_contents);

  // Get the points corresponding to the center of the browser window in both
  // screen coordinates and window coordinates.
  const gfx::Rect window_in_screen_coords =
      render_widget_host_impl->GetView()->GetBoundsInRootWindow();
  const gfx::PointF screen_point =
      gfx::PointF(window_in_screen_coords.CenterPoint());
  const gfx::PointF client_point =
      gfx::PointF(window_in_screen_coords.width() / 2,
                  window_in_screen_coords.height() / 2);

  // Drop the test file.
  DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.filenames.emplace_back(
      ui::FileInfo(test_file_path, test_file_path.BaseName()));
  render_widget_host_impl->FilterDropData(&drop_data);
  render_widget_host_impl->DragTargetDragEnter(
      drop_data, client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDragOver(
      client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDrop(drop_data, client_point, screen_point,
                                          /*key_modifiers=*/0,
                                          base::DoNothing());

  // Expect the promise to resolve with `test_contents`.
  EXPECT_EQ(test_contents, EvalJs(shell(), "p"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessDragDropBrowserTest, DropDirectory) {
  // Get the RenderWidgetHostImpl for the main (and only) frame.
  RenderWidgetHostImpl* render_widget_host_impl =
      GetRenderWidgetHostImplForMainFrame();
  DCHECK(render_widget_host_impl);

  // Prepare the window for dragging and dropping.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Prevent defaults of drag operations and create a promise that will resolve
  // with the name of the child directory of the dropped directory. The promise
  // rejects if there are zero/multiple items dropped or if the dropped item is
  // directory. The promise will also reject if there are zero/multiple items
  // inside the dropped directory.
  ASSERT_TRUE(
      ExecJs(shell(),
             "window.ondragenter = (e) => { e.preventDefault() };"
             "window.ondragover = (e) => { e.preventDefault() };"
             "var p = new Promise((resolve, reject) => {"
             "  window.ondrop = async (dragEvent) => {"
             "    dragEvent.preventDefault();"
             "    if (dragEvent.dataTransfer.items.length !== 1) {"
             "      reject('There were ' + dragEvent.dataTransfer.items.length"
             "              + ' dropped items. Expected 1.');"
             "    }"
             "    if (dragEvent.dataTransfer.items[0].kind != 'file') {"
             "      reject('The drag item was of kind: ' +"
             "              dragEvent.dataTransfer.items[0].kind + '. Expected"
             "              file.');"
             "    }"
             "    const fileItem = dragEvent.dataTransfer.items[0];"
             "    directoryHandle = await fileItem.getAsFileSystemHandle();"
             "    if (directoryHandle.kind !== 'directory') {"
             "       reject('The dragged item was a file, expected it to be a "
             "               directory.');"
             "    }"
             "    let directoryContents = [];"
             "    for await (let fileName of directoryHandle.keys()) {"
             "       directoryContents.push(fileName);"
             "     };"
             "     if (directoryContents.length !== 1) {"
             "       reject('There were ', directoryContents.length, ' files "
             "               in the dropped directory. Expected 1.');"
             "     }"
             "     resolve(directoryContents.pop());"
             "   }"
             "});"));

  // Create a directory and create a file inside the directory.
  const base::FilePath test_dir_path = CreateTestDir();
  std::string contents = "Irrelevant contents.";
  const base::FilePath file_inside_dir =
      CreateTestFileInDirectory(test_dir_path, contents);

  // Get the points corresponding to the center of the browser window in both
  // screen coordinates and window coordinates.
  const gfx::Rect window_in_screen_coords =
      render_widget_host_impl->GetView()->GetBoundsInRootWindow();
  const gfx::PointF screen_point =
      gfx::PointF(window_in_screen_coords.CenterPoint());
  const gfx::PointF client_point =
      gfx::PointF(window_in_screen_coords.width() / 2,
                  window_in_screen_coords.height() / 2);

  // Drop the test file.
  DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.filenames.emplace_back(
      ui::FileInfo(test_dir_path, test_dir_path.BaseName()));
  render_widget_host_impl->FilterDropData(&drop_data);
  render_widget_host_impl->DragTargetDragEnter(
      drop_data, client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDragOver(
      client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDrop(drop_data, client_point, screen_point,
                                          /*key_modifiers=*/0,
                                          base::DoNothing());

  // Wait promise to resolve and expect the directory to have child with name
  // matching the base name of `file_inside_dir`.
  EXPECT_EQ(file_inside_dir.BaseName().AsUTF8Unsafe(), EvalJs(shell(), "p"));
}
}  // namespace content
