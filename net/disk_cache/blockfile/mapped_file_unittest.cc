// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Implementation of FileIOCallback for the tests.
class FileCallbackTest: public disk_cache::FileIOCallback {
 public:
  FileCallbackTest(int id, MessageLoopHelper* helper, int* max_id)
      : id_(id),
        helper_(helper),
        max_id_(max_id) {
  }
  ~FileCallbackTest() override = default;

  void OnFileIOComplete(int bytes_copied) override;

 private:
  int id_;
  raw_ptr<MessageLoopHelper> helper_;
  raw_ptr<int> max_id_;
};

void FileCallbackTest::OnFileIOComplete(int bytes_copied) {
  if (id_ > *max_id_) {
    NOTREACHED();
  }

  helper_->CallbackWasCalled();
}

}  // namespace

TEST_F(DiskCacheTest, MappedFile_SyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  auto file = base::MakeRefCounted<disk_cache::MappedFile>();
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  char buffer1[20];
  char buffer2[20];
  auto buffer1_span = base::as_writable_byte_span(buffer1);
  CacheTestFillBuffer(buffer1_span, false);
  buffer1_span.copy_prefix_from(
      base::byte_span_with_nul_from_cstring("the data"));
  EXPECT_TRUE(file->Write(base::as_byte_span(buffer1), 8192));
  EXPECT_TRUE(file->Read(base::as_writable_byte_span(buffer2), 8192));
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_AsyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  auto file = base::MakeRefCounted<disk_cache::MappedFile>();
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  int max_id = 0;
  MessageLoopHelper helper;
  FileCallbackTest callback(1, &helper, &max_id);

  char buffer1[20];
  char buffer2[20];
  auto buffer1_span = base::as_writable_byte_span(buffer1);
  CacheTestFillBuffer(buffer1_span, false);
  buffer1_span.copy_prefix_from(
      base::byte_span_with_nul_from_cstring("the data"));
  bool completed;
  EXPECT_TRUE(file->Write(base::as_byte_span(buffer1), 1024 * 1024, &callback,
                          &completed));
  int expected = completed ? 0 : 1;

  max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Read(base::as_writable_byte_span(buffer2), 1024 * 1024,
                         &callback, &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, helper.callbacks_called());
  EXPECT_FALSE(helper.callback_reused_error());
  EXPECT_STREQ(buffer1, buffer2);
}
