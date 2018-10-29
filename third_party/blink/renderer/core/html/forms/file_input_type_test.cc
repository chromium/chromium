// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/file_input_type.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"

namespace blink {

namespace {

class WebKitDirectoryChromeClient : public EmptyChromeClient {
 public:
  void EnumerateChosenDirectory(FileChooser* chooser) override {
    chooser->AddRef();  // Do same as ChromeClientImpl
    static_cast<WebFileChooserCompletion*>(chooser)->DidChooseFile(
        WebVector<WebString>());
  }

  void RegisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED() << "RegisterPopupOpeningObserver should not be called.";
  }
  void UnregisterPopupOpeningObserver(PopupOpeningObserver*) override {
    NOTREACHED() << "UnregisterPopupOpeningObserver should not be called.";
  }
};

}  // namespace

TEST(FileInputTypeTest, createFileList) {
  FileChooserFileInfoList files;

  // Native file.
  files.push_back(CreateFileChooserFileInfoNative("/native/path/native-file",
                                                  "display-name"));

  // Non-native file.
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  files.push_back(CreateFileChooserFileInfoFileSystem(
      url, base::Time::FromJsTime(1.0 * kMsPerDay + 3), 64));

  FileList* list = FileInputType::CreateFileList(files, false);
  ASSERT_TRUE(list);
  ASSERT_EQ(2u, list->length());

  EXPECT_EQ("/native/path/native-file", list->item(0)->GetPath());
  EXPECT_EQ("display-name", list->item(0)->name());
  EXPECT_TRUE(list->item(0)->FileSystemURL().IsEmpty());

  EXPECT_TRUE(list->item(1)->GetPath().IsEmpty());
  EXPECT_EQ("non-native-file", list->item(1)->name());
  EXPECT_EQ(url, list->item(1)->FileSystemURL());
  EXPECT_EQ(64u, list->item(1)->size());
  EXPECT_EQ(1.0 * kMsPerDay + 3, list->item(1)->lastModified());
}

TEST(FileInputTypeTest, ignoreDroppedNonNativeFiles) {
  Document* document = Document::CreateForTest();
  auto* input = HTMLInputElement::Create(*document, CreateElementFlags());
  InputType* file_input = FileInputType::Create(*input);

  DataObject* native_file_raw_drag_data = DataObject::Create();
  const DragData native_file_drag_data(native_file_raw_drag_data, FloatPoint(),
                                       FloatPoint(), kDragOperationCopy);
  native_file_drag_data.PlatformData()->Add(File::Create("/native/path"));
  native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&native_file_drag_data);
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());

  DataObject* non_native_file_raw_drag_data = DataObject::Create();
  const DragData non_native_file_drag_data(non_native_file_raw_drag_data,
                                           FloatPoint(), FloatPoint(),
                                           kDragOperationCopy);
  FileMetadata metadata;
  metadata.length = 1234;
  const KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  non_native_file_drag_data.PlatformData()->Add(
      File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible));
  non_native_file_drag_data.PlatformData()->SetFilesystemId("fileSystemId");
  file_input->ReceiveDroppedFiles(&non_native_file_drag_data);
  // Dropping non-native files should not change the existing files.
  EXPECT_EQ("fileSystemId", file_input->DroppedFileSystemId());
  ASSERT_EQ(1u, file_input->Files()->length());
  EXPECT_EQ(String("/native/path"), file_input->Files()->item(0)->GetPath());
}

TEST(FileInputTypeTest, setFilesFromPaths) {
  Document* document = Document::CreateForTest();
  auto* input = HTMLInputElement::Create(*document, CreateElementFlags());
  InputType* file_input = FileInputType::Create(*input);
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
  input->SetBooleanAttribute(HTMLNames::multipleAttr, true);
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
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  auto* chrome_client = new WebKitDirectoryChromeClient;
  page_clients.chrome_client = chrome_client;
  auto page_holder = DummyPageHolder::Create(IntSize(), &page_clients);
  Document& doc = page_holder->GetDocument();

  doc.body()->SetInnerHTMLFromString("<input type=file webkitdirectory>");
  auto& input = *ToHTMLInputElement(doc.body()->firstChild());

  DragData drag_data(DataObject::Create(), FloatPoint(), FloatPoint(),
                     kDragOperationCopy);
  drag_data.PlatformData()->Add(File::Create("/foo/bar"));
  input.ReceiveDroppedFiles(&drag_data);

  // The test passes if WebKitDirectoryChromeClient::
  // UnregisterPopupOpeningObserver() was not called.
}

}  // namespace blink
