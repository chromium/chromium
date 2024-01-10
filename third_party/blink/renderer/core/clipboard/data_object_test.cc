// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/data_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/clipboard/data_object_item.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class DataObjectTest : public testing::Test {
 public:
  DataObjectTest() : data_object_(DataObject::Create()) {}

 protected:
  test::TaskEnvironment task_environment;

  Persistent<DataObject> data_object_;
};

class DataObjectObserver : public GarbageCollected<DataObjectObserver>,
                           public DataObject::Observer {
 public:
  DataObjectObserver() : call_count_(0) {}
  void OnItemListChanged() override { call_count_++; }
  size_t call_count() { return call_count_; }

 private:
  size_t call_count_;
};

TEST_F(DataObjectTest, DataObjectObserver) {
  ScopedNullExecutionContext context;
  DataObjectObserver* observer = MakeGarbageCollected<DataObjectObserver>();
  data_object_->AddObserver(observer);

  data_object_->ClearAll();
  EXPECT_EQ(0U, data_object_->length());
  EXPECT_EQ(0U, observer->call_count());

  data_object_->SetData("text/plain", "foobar");
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(1U, observer->call_count());

  DataObjectItem* item = data_object_->Add("bar quux", "text/plain");
  EXPECT_EQ(nullptr, item);
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(1U, observer->call_count());

  item = data_object_->Add("bar quux", "application/octet-stream");
  EXPECT_NE(nullptr, item);
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(2U, observer->call_count());

  data_object_->DeleteItem(42);
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(2U, observer->call_count());

  data_object_->DeleteItem(0);
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(3U, observer->call_count());

  DataObjectObserver* observer2 = MakeGarbageCollected<DataObjectObserver>();
  data_object_->AddObserver(observer2);

  String file_path =
      test::BlinkRootDir() + "/renderer/core/clipboard/data_object_test.cc";
  data_object_->AddFilename(&context.GetExecutionContext(), file_path, String(),
                            String());
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(4U, observer->call_count());
  EXPECT_EQ(1U, observer2->call_count());

  data_object_->ClearData("application/octet-stream");
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(5U, observer->call_count());
  EXPECT_EQ(2U, observer2->call_count());

  data_object_->ClearStringItems();
  EXPECT_EQ(1U, data_object_->length());
  EXPECT_EQ(5U, observer->call_count());
  EXPECT_EQ(2U, observer2->call_count());

  item = data_object_->Add("new plain item", "text/plain");
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(6U, observer->call_count());
  EXPECT_EQ(3U, observer2->call_count());

  item = data_object_->Add("new data item", "Files");
  EXPECT_EQ(3U, data_object_->length());
  EXPECT_EQ(7U, observer->call_count());
  EXPECT_EQ(4U, observer2->call_count());

  String file_path2 =
      test::BlinkRootDir() + "/renderer/core/clipboard/data_object_test.h";
  data_object_->AddFilename(&context.GetExecutionContext(), file_path2,
                            String(), String());
  EXPECT_EQ(4U, data_object_->length());
  EXPECT_EQ(8U, observer->call_count());
  EXPECT_EQ(5U, observer2->call_count());

  data_object_->ClearData("Files");
  EXPECT_EQ(3U, data_object_->length());
  EXPECT_EQ(9U, observer->call_count());
  EXPECT_EQ(6U, observer2->call_count());

  data_object_->ClearStringItems();
  EXPECT_EQ(2U, data_object_->length());
  EXPECT_EQ(10U, observer->call_count());
  EXPECT_EQ(7U, observer2->call_count());

  data_object_->ClearAll();
  EXPECT_EQ(0U, data_object_->length());
  EXPECT_EQ(11U, observer->call_count());
  EXPECT_EQ(8U, observer2->call_count());
}

TEST_F(DataObjectTest, addItemWithFilenameAndNoTitle) {
  ScopedNullExecutionContext context;
  String file_path =
      test::BlinkRootDir() + "/renderer/core/clipboard/data_object_test.cc";

  data_object_->AddFilename(&context.GetExecutionContext(), file_path, String(),
                            String());
  EXPECT_EQ(1U, data_object_->length());

  DataObjectItem* item = data_object_->Item(0);
  EXPECT_EQ(DataObjectItem::kFileKind, item->Kind());

  Blob* blob = item->GetAsFile();
  ASSERT_TRUE(blob->IsFile());
  auto* file = DynamicTo<File>(blob);
  ASSERT_TRUE(file);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
}

TEST_F(DataObjectTest, addItemWithFilenameAndTitle) {
  ScopedNullExecutionContext context;
  String file_path =
      test::BlinkRootDir() + "/renderer/core/clipboard/data_object_test.cc";

  data_object_->AddFilename(&context.GetExecutionContext(), file_path,
                            "name.cpp", String());
  EXPECT_EQ(1U, data_object_->length());

  DataObjectItem* item = data_object_->Item(0);
  EXPECT_EQ(DataObjectItem::kFileKind, item->Kind());

  Blob* blob = item->GetAsFile();
  auto* file = DynamicTo<File>(blob);
  ASSERT_TRUE(file);
  EXPECT_TRUE(file->HasBackingFile());
  EXPECT_EQ(File::kIsUserVisible, file->GetUserVisibility());
  EXPECT_EQ(file_path, file->GetPath());
  EXPECT_EQ("name.cpp", file->name());
}

TEST_F(DataObjectTest, fileSystemId) {
  ScopedNullExecutionContext context;
  String file_path =
      test::BlinkRootDir() + "/renderer/core/clipboard/data_object_test.cpp";
  KURL url;

  data_object_->AddFilename(&context.GetExecutionContext(), file_path, String(),
                            String());
  data_object_->AddFilename(&context.GetExecutionContext(), file_path, String(),
                            "fileSystemIdForFilename");
  FileMetadata metadata;
  metadata.length = 0;
  data_object_->Add(
      File::CreateForFileSystemFile(url, metadata, File::kIsUserVisible,
                                    BlobDataHandle::Create()),
      "fileSystemIdForFileSystemFile");

  ASSERT_EQ(3U, data_object_->length());

  {
    DataObjectItem* item = data_object_->Item(0);
    EXPECT_FALSE(item->HasFileSystemId());
  }

  {
    DataObjectItem* item = data_object_->Item(1);
    EXPECT_TRUE(item->HasFileSystemId());
    EXPECT_EQ("fileSystemIdForFilename", item->FileSystemId());
  }

  {
    DataObjectItem* item = data_object_->Item(2);
    EXPECT_TRUE(item->HasFileSystemId());
    EXPECT_EQ("fileSystemIdForFileSystemFile", item->FileSystemId());
  }
}

}  // namespace blink
