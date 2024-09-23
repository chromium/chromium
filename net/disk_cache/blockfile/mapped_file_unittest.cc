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
    NOTREACHED_IN_MIGRATION();
    helper_->set_callback_reused_error(true);
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
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", std::size(buffer1));
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 8192));
  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 8192));
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
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", std::size(buffer1));
  bool completed;
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 1024 * 1024, &callback,
              &completed));
  int expected = completed ? 0 : 1;

  max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 1024 * 1024, &callback,
              &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, helper.callbacks_called());
  EXPECT_FALSE(helper.callback_reused_error());
  EXPECT_STREQ(buffer1, buffer2);
}
