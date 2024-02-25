// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace content {

// End-to-end tests for drag and drop, including correct behavior of the
// DataTransferItem's getAsFile method.

class FileSystemURLDragDropBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(embedded_test_server()->Start());
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

  RenderWidgetHostImpl* GetRenderWidgetHostImplForMainFrame() {
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemURLDragDropBrowserTest, FileSystemFileDrop) {
  // Get the RenderWidgetHostImpl for the main (and only) frame.
  RenderWidgetHostImpl* render_widget_host_impl =
      GetRenderWidgetHostImplForMainFrame();
  DCHECK(render_widget_host_impl);

  // Prepare the window for dragging and dropping.
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Prevent defaults of drag operations and create a promise that will resolve
  // with the text from a dropped file after window.ondrop is called. The text
  // is retrieved using the DataTransferItem getAsFile function. The promise
  // will reject if zero/multiples items are dropped or if the item is not a
  // file. This test will also ensure that certain drag handlers get an event
  // with the DataTransferItemList populated (in the language of the spec,
  // handlers where the expected "drag data store mode" is "protected mode").
  ASSERT_TRUE(
      ExecJs(shell(),
             "const checkDataTransfer = (caller, event, reject) => {"
             "  if (event.dataTransfer.items.length !== 1) {"
             "    reject('There were ' + event.dataTransfer.items.length"
             "            + ' DataTransferItems in the list passed to the '"
             "            + caller + ' handler. Expected 1.');"
             "  }"
             "  if (event.dataTransfer.items[0].kind != 'file') {"
             "    reject('The DataTransferItem was of kind: '"
             "            + event.dataTransfer.items[0].kind + ' (in the '"
             "            + caller + ' handler). Expected file.');"
             "  }"
             "};"
             "const handled_events = [];"
             "const expected_events = ["
             "  'ondragenter',"
             "  'ondragover',"
             "  'ondrop',"
             "];"
             "var p = new Promise((resolve, reject) => {"
             "  window.ondragenter = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondragenter', event, reject);"
             "    handled_events.push('ondragenter');"
             "  };"
             "  window.ondragover = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondragover', event, reject);"
             "    handled_events.push('ondragover');"
             "  };"
             "  window.ondrop = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondrop', event, reject);"
             "    handled_events.push('ondrop');"
             "    if (handled_events.length != expected_events.length) {"
             "      reject('Unexpected number of events handled: ' +"
             "              handled_events.length);"
             "    }"
             "    for (var i = 0; i < handled_events.length; i++) {"
             "      if (handled_events[i] != expected_events[i]) {"
             "        reject('Unexpected order of drag/drop handlers');"
             "      }"
             "    }"
             "    const fileItem = event.dataTransfer.items[0];"
             "    const file = fileItem.getAsFile();"
             "    var text = await file.text();"
             "    resolve(text);"
             "  }"
             "});"));

  // Create a directory and create a file inside the directory.
  const base::FilePath test_dir_path = CreateTestDir();
  std::string test_contents = "Debugged code is the best code.";
  const base::FilePath file_inside_dir =
      CreateTestFileInDirectory(test_dir_path, test_contents);

  // Create a File System File from this local file
  storage::ExternalMountPoints* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  constexpr char testMountName[] = "DropFileSystemFileTestMount";

  EXPECT_TRUE(external_mount_points->RegisterFileSystem(
      testMountName, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), test_dir_path));

  storage::FileSystemURL original_file =
      external_mount_points->CreateExternalFileSystemURL(
          blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
          testMountName, file_inside_dir.BaseName());
  EXPECT_TRUE(original_file.is_valid());

  // Get the points corresponding to the center of the browser window in
  // both screen coordinates and window coordinates.
  const gfx::Rect window_in_screen_coords =
      render_widget_host_impl->GetView()->GetBoundsInRootWindow();
  const gfx::PointF screen_point =
      gfx::PointF(window_in_screen_coords.CenterPoint());
  const gfx::PointF client_point =
      gfx::PointF(window_in_screen_coords.width() / 2,
                  window_in_screen_coords.height() / 2);

  // Drop the test file.
  DropData::FileSystemFileInfo filesystem_file_info;
  filesystem_file_info.url = original_file.ToGURL();
  filesystem_file_info.size = test_contents.size();
  filesystem_file_info.filesystem_id = original_file.filesystem_id();
  DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.file_system_files.push_back(filesystem_file_info);

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

  EXPECT_TRUE(external_mount_points->RevokeFileSystem(testMountName));
}

IN_PROC_BROWSER_TEST_F(FileSystemURLDragDropBrowserTest, FileSystemFileLeave) {
  // Get the RenderWidgetHostImpl for the main (and only) frame.
  RenderWidgetHostImpl* render_widget_host_impl =
      GetRenderWidgetHostImplForMainFrame();
  DCHECK(render_widget_host_impl);

  // Prepare the window for dragging and dropping.
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));

  // Prevent defaults of drag operations and create a promise that will resolve
  // once window.ondragleave is called. The promise will reject if the
  // DataTransferItemList in the event object passed to the ondragenter,
  // ondragover, and ondragleave handlers contains zero/multiple items, or if
  // the sole DataTransferItem has a `kind` that is not 'file'. This ensures
  // that sufficient state is preserved across drag events to provide this data
  // to JS.
  ASSERT_TRUE(
      ExecJs(shell(),
             "const checkDataTransfer = (caller, event, reject) => {"
             "  if (event.dataTransfer.items.length !== 1) {"
             "    reject('There were ' + event.dataTransfer.items.length"
             "            + ' DataTransferItems in the list passed to the '"
             "            + caller + ' handler. Expected 1.');"
             "  }"
             "  if (event.dataTransfer.items[0].kind != 'file') {"
             "    reject('The DataTransferItem was of kind: '"
             "            + event.dataTransfer.items[0].kind + ' (in the '"
             "            + caller + ' handler). Expected file.');"
             "  }"
             "};"
             "const handled_events = [];"
             "const expected_events = ["
             "  'ondragenter',"
             "  'ondragover',"
             "  'ondragleave',"
             "];"
             "var p = new Promise((resolve, reject) => {"
             "  window.ondragenter = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondragenter', event, reject);"
             "    handled_events.push('ondragenter');"
             "  };"
             "  window.ondragover = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondragover', event, reject);"
             "    handled_events.push('ondragover');"
             "  };"
             "  window.ondragleave = async (event) => {"
             "    event.preventDefault();"
             "    checkDataTransfer('ondragleave', event, reject);"
             "    handled_events.push('ondragleave');"
             "    if (handled_events.length != expected_events.length) {"
             "      reject('Unexpected number of events handled: ' +"
             "              handled_events.length);"
             "    }"
             "    for (var i = 0; i < handled_events.length; i++) {"
             "      if (handled_events[i] != expected_events[i]) {"
             "        reject('Unexpected order of drag/drop handlers');"
             "      }"
             "    }"
             "    resolve('done');"
             "  }"
             "});"));

  // Create a directory and create a file inside the directory.
  const base::FilePath test_dir_path = CreateTestDir();
  std::string test_contents = "Irrelevant contents.";
  const base::FilePath file_inside_dir =
      CreateTestFileInDirectory(test_dir_path, test_contents);

  // Create a File System File from this local file
  storage::ExternalMountPoints* external_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  constexpr char testMountName[] = "LeaveFileSystemFileTestMount";

  EXPECT_TRUE(external_mount_points->RegisterFileSystem(
      testMountName, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), test_dir_path));

  storage::FileSystemURL original_file =
      external_mount_points->CreateExternalFileSystemURL(
          blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
          testMountName, file_inside_dir.BaseName());
  EXPECT_TRUE(original_file.is_valid());

  // Get the points corresponding to the center of the browser window in
  // both screen coordinates and window coordinates.
  const gfx::Rect window_in_screen_coords =
      render_widget_host_impl->GetView()->GetBoundsInRootWindow();
  const gfx::PointF screen_point =
      gfx::PointF(window_in_screen_coords.CenterPoint());
  const gfx::PointF client_point =
      gfx::PointF(window_in_screen_coords.width() / 2,
                  window_in_screen_coords.height() / 2);

  DropData::FileSystemFileInfo filesystem_file_info;
  filesystem_file_info.url = original_file.ToGURL();
  filesystem_file_info.size = test_contents.size();
  filesystem_file_info.filesystem_id = original_file.filesystem_id();
  DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.file_system_files.push_back(filesystem_file_info);

  render_widget_host_impl->FilterDropData(&drop_data);
  render_widget_host_impl->DragTargetDragEnter(
      drop_data, client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDragOver(
      client_point, screen_point,
      blink::DragOperationsMask::kDragOperationEvery,
      /*key_modifiers=*/0, base::DoNothing());
  render_widget_host_impl->DragTargetDragLeave(client_point, screen_point);

  // Expect the promise to resolve with `done`.
  EXPECT_EQ("done", EvalJs(shell(), "p"));

  EXPECT_TRUE(external_mount_points->RevokeFileSystem(testMountName));
}
}  // namespace content
