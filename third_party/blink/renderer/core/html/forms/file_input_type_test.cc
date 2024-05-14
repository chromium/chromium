// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/file_input_type.h"

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/mock_file_chooser.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

class MockEventListener final : public NativeEventListener {
 public:
  bool invoked = false;
  void Invoke(ExecutionContext*, Event*) override { invoked = true; }
  void ClearInvoked() { invoked = false; }
};

class WebKitDirectoryChromeClient : public EmptyChromeClient {
 public:
  void RegisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED_IN_MIGRATION()
        << "RegisterPopupOpeningObserver should not be called.";
  }
  void UnregisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED_IN_MIGRATION()
        << "UnregisterPopupOpeningObserver should not be called.";
  }
};

}  // namespace

TEST(FileInputTypeTest, createFileList) {
  test::TaskEnvironment task_environment;
  FileChooserFileInfoList files;

  // Native file.
  files.push_back(CreateFileChooserFileInfoNative("/native/path/native-file",
                                                  "display-name"));

  // Non-native file.
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  files.push_back(CreateFileChooserFileInfoFileSystem(
      url, base::Time::FromMillisecondsSinceUnixEpoch(1.0 * kMsPerDay + 3),
      64));

  ScopedNullExecutionContext execution_context;
  FileList* list = FileInputType::CreateFileList(
      execution_context.GetExecutionContext(), files, base::FilePath());
  ASSERT_TRUE(list);
  ASSERT_EQ(2u, list->length());

  EXPECT_EQ("/native/path/native-file", list->item(0)->GetPath());
  EXPECT_EQ("display-name", list->item(0)->name());
  EXPECT_TRUE(list->item(0)->FileSystemURL().IsEmpty());

  EXPECT_TRUE(list->item(1)->GetPath().empty());
  EXPECT_EQ("non-native-file", list->item(1)->name());
  EXPECT_EQ(url, list->item(1)->FileSystemURL());
  EXPECT_EQ(64u, list->item(1)->size());
  EXPECT_EQ(1.0 * kMsPerDay + 3, list->item(1)->lastModified());
}

TEST(FileInputTypeTest, ignoreDroppedNonNativeFiles) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);

  DataObject* native_file_raw_drag_data = DataObject::Create();
  const DragData native_file_drag_data(native_file_raw_drag_data, gfx::PointF(),
                                       gfx::PointF(), kDragOperationCopy,
                                       false);
  native_file_drag_data.PlatformData()->Add(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/native/path"));
  native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&native_file_drag_data);
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  DataObject* non_native_file_raw_drag_data = DataObject::Create();
  const DragData non_native_file_drag_data(non_native_file_raw_drag_data,
                                           gfx::PointF(), gfx::PointF(),
                                           kDragOperationCopy, false);
  FileMetadata metadata;
  metadata.length = 1234;
  const KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  non_native_file_drag_data.PlatformData()->Add(File::CreateForFileSystemFile(
      url, metadata, File::kIsUserVisible, BlobDataHandle::Create()));
  non_native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&non_native_file_drag_data);
  // Dropping non-native files should not change the existing files.
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());
}

TEST(FileInputTypeTest, setFilesFromPaths) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  Vector<String> paths;
  paths.push_back("/native/path");
  paths.push_back("/native/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  // Try to upload multiple files without multipleAttr
  paths.clear();
  paths.push_back("/native/path1");
  paths.push_back("/native/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path1"), file_input->Files()->item(0)->GetPath());

  // Try to upload multiple files with multipleAttr
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);
  paths.clear();
  paths.push_back("/native/real/path1");
  paths.push_back("/native/real/path2");
  file_input->SetFilesFromPaths(paths);
  ASSERT_EQ(2u, file_input->Files()->length());
  EXPECT_EQ(String("/native/real/path1"),
            file_input->Files()->item(0)->GetPath());
  EXPECT_EQ(String("/native/real/path2"),
            file_input->Files()->item(1)->GetPath());
}

TEST(FileInputTypeTest, DropTouchesNoPopupOpeningObserver) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* chrome_client = MakeGarbageCollected<WebKitDirectoryChromeClient>();
  auto page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(), chrome_client);
  Document& doc = page_holder->GetDocument();

  doc.body()->setInnerHTML("<input type=file webkitdirectory>");
  auto& input = *To<HTMLInputElement>(doc.body()->firstChild());

  base::RunLoop run_loop;
  MockFileChooser chooser(doc.GetFrame()->GetBrowserInterfaceBroker(),
                          run_loop.QuitClosure());
  DragData drag_data(DataObject::Create(), gfx::PointF(), gfx::PointF(),
                     kDragOperationCopy, false);
  drag_data.PlatformData()->Add(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/foo/bar"));
  input.ReceiveDroppedFiles(&drag_data);
  run_loop.Run();

  chooser.ResponseOnOpenFileChooser(FileChooserFileInfoList());

  // The test passes if WebKitDirectoryChromeClient::
  // UnregisterPopupOpeningObserver() was not called.
}

TEST(FileInputTypeTest, BeforePseudoCrash) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<DummyPageHolder> page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& doc = page_holder->GetDocument();
  doc.documentElement()->setInnerHTML(R"HTML(
<style>
.c6 {
  zoom: 0.01;
}

.c6::first-letter {
  position: fixed;
  border-style: groove;
}

.c6::before {
  content: 'c6';
}

.c7 {
  zoom: 0.1;
}

.c7::first-letter {
  position: fixed;
  border-style: groove;
}

.c7::before {
  content: 'c7';
}

</style>
<input type=file class=c6>
<input type=file class=c7>
)HTML");
  doc.View()->UpdateAllLifecyclePhasesForTest();
  // The test passes if no CHECK failures and no null pointer dereferences.
}

TEST(FileInputTypeTest, ChangeTypeDuringOpeningFileChooser) {
  test::TaskEnvironment task_environment;
  // We use WebViewHelper instead of DummyPageHolder, in order to use
  // ChromeClientImpl.
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();
  LocalFrame* frame = helper.LocalMainFrame()->GetFrame();

  Document& doc = *frame->GetDocument();
  doc.body()->setInnerHTML("<input type=file>");
  auto& input = *To<HTMLInputElement>(doc.body()->firstChild());

  base::RunLoop run_loop;
  MockFileChooser chooser(frame->GetBrowserInterfaceBroker(),
                          run_loop.QuitClosure());

  // Calls MockFileChooser::OpenFileChooser().
  LocalFrame::NotifyUserActivation(
      frame, mojom::blink::UserActivationNotificationType::kInteraction);
  input.click();
  run_loop.Run();

  input.setType(input_type_names::kColor);

  FileChooserFileInfoList list;
  list.push_back(CreateFileChooserFileInfoNative("/path/to/file.txt", ""));
  chooser.ResponseOnOpenFileChooser(std::move(list));

  // Receiving a FileChooser response should not alter a shadow tree
  // for another type.
  EXPECT_TRUE(IsA<HTMLElement>(
      input.EnsureShadowSubtree()->firstChild()->firstChild()));
}

// Tests selecting same file twice should fire cancel event second time.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsForSameFile) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  auto reset = [&] {
    listener_change->ClearInvoked();
    listener_cancel->ClearInvoked();
  };

  auto* const selection_1 = MakeGarbageCollected<FileList>();
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  file_input->SetFilesAndDispatchEvents(selection_1);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);

  reset();
  auto* const selection_2 = MakeGarbageCollected<FileList>();
  selection_2->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  file_input->SetFilesAndDispatchEvents(selection_2);
  EXPECT_FALSE(listener_change->invoked);
  EXPECT_TRUE(listener_cancel->invoked);
}

// Tests selecting same files twice should fire cancel event second time.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsForSameFiles) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);
  auto reset = [&] {
    listener_change->ClearInvoked();
    listener_cancel->ClearInvoked();
  };

  auto* const selection_1 = MakeGarbageCollected<FileList>();
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/B.txt"));
  file_input->SetFilesAndDispatchEvents(selection_1);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);

  reset();
  auto* const selection_2 = MakeGarbageCollected<FileList>();
  selection_2->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  selection_2->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/B.txt"));
  file_input->SetFilesAndDispatchEvents(selection_2);
  EXPECT_FALSE(listener_change->invoked);
  EXPECT_TRUE(listener_cancel->invoked);
}

// Tests selecting different file after first selection should fire change
// event.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsForDifferentFile) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  auto reset = [&] {
    listener_change->ClearInvoked();
    listener_cancel->ClearInvoked();
  };

  auto* const selection_1 = MakeGarbageCollected<FileList>();
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  file_input->SetFilesAndDispatchEvents(selection_1);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);

  reset();
  auto* const selection_2 = MakeGarbageCollected<FileList>();
  selection_2->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/B.txt"));
  file_input->SetFilesAndDispatchEvents(selection_2);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);
}

// Tests selecting different files after first selection should fire change
// event.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsForDifferentFiles) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);
  auto reset = [&] {
    listener_change->ClearInvoked();
    listener_cancel->ClearInvoked();
  };

  auto* const selection_1 = MakeGarbageCollected<FileList>();
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/B.txt"));
  file_input->SetFilesAndDispatchEvents(selection_1);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);

  reset();
  auto* const selection_2 = MakeGarbageCollected<FileList>();
  selection_2->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  file_input->SetFilesAndDispatchEvents(selection_2);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);
}

// Tests clearing selection (click cancel in file chooser) after selection
// should fire change event.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsCancelWithSelection) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);
  auto reset = [&] {
    listener_change->ClearInvoked();
    listener_cancel->ClearInvoked();
  };

  auto* const selection_1 = MakeGarbageCollected<FileList>();
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/A.txt"));
  selection_1->Append(MakeGarbageCollected<File>(
      &execution_context.GetExecutionContext(), "/path/to/B.txt"));
  file_input->SetFilesAndDispatchEvents(selection_1);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);

  reset();
  auto* const selection_2 = MakeGarbageCollected<FileList>();
  file_input->SetFilesAndDispatchEvents(selection_2);
  EXPECT_TRUE(listener_change->invoked);
  EXPECT_FALSE(listener_cancel->invoked);
}

// Tests clearing selection (click cancel in file chooser) without selection
// should fire cancel event.
TEST(FileInputTypeTest, SetFilesFireCorrectEventsCancelWithoutSelection) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;

  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  auto* input = MakeGarbageCollected<HTMLInputElement>(*document);
  InputType* file_input = MakeGarbageCollected<FileInputType>(*input);
  auto* listener_change = MakeGarbageCollected<MockEventListener>();
  auto* listener_cancel = MakeGarbageCollected<MockEventListener>();
  input->addEventListener(event_type_names::kChange, listener_change);
  input->addEventListener(event_type_names::kCancel, listener_cancel);
  input->SetBooleanAttribute(html_names::kMultipleAttr, true);

  auto* const selection = MakeGarbageCollected<FileList>();
  file_input->SetFilesAndDispatchEvents(selection);
  EXPECT_FALSE(listener_change->invoked);
  EXPECT_TRUE(listener_cancel->invoked);
}

}  // namespace blink
