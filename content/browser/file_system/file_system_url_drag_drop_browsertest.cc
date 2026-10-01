// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "url/origin.h"

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
      render_widget_host_impl->GetView()->GetBoundsInScreen();
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
      render_widget_host_impl->GetView()->GetBoundsInScreen();
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

IN_PROC_BROWSER_TEST_F(FileSystemURLDragDropBrowserTest,
                       IsolatedCopyMoveErrorPrecedence) {
  const GURL page_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), page_url));
  auto* process = static_cast<RenderProcessHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());

  storage::IsolatedContext::FileInfoSet files;
  const base::FilePath file_path =
      CreateTestFileInDirectory(temp_dir_.GetPath(), "test");
  ASSERT_TRUE(files.AddPathWithName(file_path, "source.txt"));
  const std::string id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  ASSERT_FALSE(id.empty());
  storage::IsolatedContext::ScopedFSHandle handle(id);
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFileSystem(
      process->GetID(), id);

  mojo::Remote<blink::mojom::FileSystemManager> manager;
  process->BindFileSystemManager(
      shell()->web_contents()->GetPrimaryMainFrame()->GetStorageKey(),
      manager.BindNewPipeAndPassReceiver());

  const GURL root = storage::GetFileSystemRootURI(
      url::Origin::Create(page_url).GetURL(), storage::kFileSystemTypeIsolated);
  const GURL source(root.spec() + id + "/source.txt");
  const GURL source_root(root.spec() + id + "/");
  const auto expect_errors = [&](const GURL& from, const GURL& to,
                                 base::File::Error expected) {
    SCOPED_TRACE(from.spec() + " -> " + to.spec());
    base::test::TestFuture<base::File::Error> copy_result;
    manager->Copy(from, to, copy_result.GetCallback());
    EXPECT_EQ(expected, copy_result.Get());
    base::test::TestFuture<base::File::Error> move_result;
    manager->Move(from, to, move_result.GetCallback());
    EXPECT_EQ(expected, move_result.Get());
  };

  expect_errors(source, GURL(root.spec() + id + "/new.txt"),
                base::File::FILE_ERROR_SECURITY);
  expect_errors(source, GURL(root.spec() + id + "/new%20name.txt"),
                base::File::FILE_ERROR_SECURITY);
  expect_errors(source, source_root, base::File::FILE_ERROR_SECURITY);
  expect_errors(source, GURL(root.spec() + id + "/source.txt/child.txt"),
                base::File::FILE_ERROR_SECURITY);
  expect_errors(source_root, GURL(root.spec() + id + "-unknown/"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "-unknown/new.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  const std::string other_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(files);
  ASSERT_FALSE(other_id.empty());
  storage::IsolatedContext::ScopedFSHandle other_handle(other_id);
  expect_errors(source, GURL(root.spec() + other_id + "/source.txt"),
                base::File::FILE_ERROR_SECURITY);
  expect_errors(source, GURL("not-a-filesystem-url"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(
      source,
      GURL("filesystem:" + url::Origin::Create(page_url).GetURL().spec() +
           "badtype/" + id + "/new.txt"),
      base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "/new%00name.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "/new%FFname.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "/new%5Cname.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "/%2Fnew.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "%2Fnew.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(source, GURL(root.spec() + id + "/%2E%2E/new.txt"),
                base::File::FILE_ERROR_INVALID_URL);
  expect_errors(
      source,
      GURL(storage::GetFileSystemRootURI(GURL("http://example.com"),
                                         storage::kFileSystemTypeIsolated)
               .spec() +
           id + "/new.txt"),
      base::File::FILE_ERROR_INVALID_URL);
  expect_errors(
      source,
      GURL(storage::GetFileSystemRootURI(url::Origin::Create(page_url).GetURL(),
                                         storage::kFileSystemTypeExternal)
               .spec() +
           id + "/new.txt"),
      base::File::FILE_ERROR_INVALID_URL);
  const GURL temporary_root =
      storage::GetFileSystemRootURI(url::Origin::Create(page_url).GetURL(),
                                    storage::kFileSystemTypeTemporary);
  expect_errors(GURL(temporary_root.spec() + "source.txt"),
                GURL(root.spec() + id + "/new.txt"),
                base::File::FILE_ERROR_INVALID_URL);

  // A different receiver StorageKey cannot validate the source URL.
  mojo::Remote<blink::mojom::FileSystemManager> other_manager;
  process->BindFileSystemManager(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(GURL("http://example.com"))),
      other_manager.BindNewPipeAndPassReceiver());
  base::test::TestFuture<base::File::Error> other_copy_result;
  other_manager->Copy(source, GURL(root.spec() + id + "/new.txt"),
                      other_copy_result.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL, other_copy_result.Get());
  base::test::TestFuture<base::File::Error> other_move_result;
  other_manager->Move(source, GURL(root.spec() + id + "/new.txt"),
                      other_move_result.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL, other_move_result.Get());

  // Copy and move have different write grants on the same isolated file
  // system. With the write grant, an unregistered sibling is still invalid.
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantCopyIntoFileSystem(
      process->GetID().GetUnsafeValue(), id);
  base::test::TestFuture<base::File::Error> writable_copy_result;
  manager->Copy(source, GURL(root.spec() + id + "/new.txt"),
                writable_copy_result.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL, writable_copy_result.Get());
  base::test::TestFuture<base::File::Error> no_delete_move_result;
  manager->Move(source, GURL(root.spec() + id + "/new.txt"),
                no_delete_move_result.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, no_delete_move_result.Get());
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantCreateFileForFileSystem(
      process->GetID().GetUnsafeValue(), id);
  ChildProcessSecurityPolicyImpl::GetInstance()->GrantDeleteFromFileSystem(
      process->GetID().GetUnsafeValue(), id);
  base::test::TestFuture<base::File::Error> writable_move_result;
  manager->Move(source, GURL(root.spec() + id + "/new.txt"),
                writable_move_result.GetCallback());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_URL, writable_move_result.Get());
}
}  // namespace content
