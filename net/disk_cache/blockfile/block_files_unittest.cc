// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// Returns the number of files in this folder.
int NumberOfFiles(const base::FilePath& path) {
  base::FileEnumerator iter(path, false, base::FileEnumerator::FILES);
  int count = 0;
  for (base::FilePath file = iter.Next(); !file.value().empty();
       file = iter.Next()) {
    count++;
  }
  return count;
}

}  // namespace

namespace disk_cache {

TEST_F(DiskCacheTest, BlockFiles_Grow) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[i]));
  }
  EXPECT_EQ(6, NumberOfFiles(cache_path_));

  // Make sure we don't keep adding files.
  for (int i = 0; i < kMaxSize * 4; i += 2) {
    int target = i % kMaxSize;
    files.DeleteBlock(address[target], false);
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[target]));
  }
  EXPECT_EQ(6, NumberOfFiles(cache_path_));
}

// We should be able to delete empty block files.
TEST_F(DiskCacheTest, BlockFiles_Shrink) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[i]));
  }

  // Now delete all the blocks, so that we can delete the two extra files.
  for (int i = 0; i < kMaxSize; i++) {
    files.DeleteBlock(address[i], false);
  }
  EXPECT_EQ(4, NumberOfFiles(cache_path_));
}

// Handling of block files not properly closed.
TEST_F(DiskCacheTest, BlockFiles_Recover) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  const int kNumEntries = 2000;
  CacheAddr entries[kNumEntries];

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  for (int i = 0; i < kNumEntries; i++) {
    Addr address(0);
    int size = (rand() % 4) + 1;
    EXPECT_TRUE(files.CreateBlock(RANKINGS, size, &address));
    entries[i] = address.value();
  }

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    CacheAddr temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    Addr address(entries[i]);
    files.DeleteBlock(address, false);
  }

  // At this point, there are kNumEntries / 2 entries on the file, randomly
  // distributed both on location and size.

  Addr address(entries[kNumEntries / 2]);
  MappedFile* file = files.GetFile(address);
  ASSERT_TRUE(nullptr != file);

  BlockFileHeader* header =
      reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(nullptr != header);

  ASSERT_EQ(0, header->updating);

  int max_entries = header->max_entries;
  int empty_1 = header->empty[0];
  int empty_2 = header->empty[1];
  int empty_3 = header->empty[2];
  int empty_4 = header->empty[3];

  // Corrupt the file.
  header->max_entries = header->empty[0] = 0;
  header->empty[1] = header->empty[2] = header->empty[3] = 0;
  header->updating = -1;

  files.CloseFiles();

  ASSERT_TRUE(files.Init(false));

  // The file must have been fixed.
  file = files.GetFile(address);
  ASSERT_TRUE(nullptr != file);

  header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(nullptr != header);

  ASSERT_EQ(0, header->updating);

  EXPECT_EQ(max_entries, header->max_entries);
  EXPECT_EQ(empty_1, header->empty[0]);
  EXPECT_EQ(empty_2, header->empty[1]);
  EXPECT_EQ(empty_3, header->empty[2]);
  EXPECT_EQ(empty_4, header->empty[3]);
}

// Handling of truncated files.
TEST_F(DiskCacheTest, BlockFiles_ZeroSizeFile) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  base::FilePath filename = files.Name(0);
  files.CloseFiles();
  // Truncate one of the files.
  {
    scoped_refptr<File> file(new File);
    ASSERT_TRUE(file->Init(filename));
    EXPECT_TRUE(file->SetLength(0));
  }

  // Initializing should fail, not crash.
  ASSERT_FALSE(files.Init(false));
}

// Handling of truncated files (non empty).
TEST_F(DiskCacheTest, BlockFiles_TruncatedFile) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));
  Addr address;
  EXPECT_TRUE(files.CreateBlock(RANKINGS, 2, &address));

  base::FilePath filename = files.Name(0);
  files.CloseFiles();
  // Truncate one of the files.
  {
    scoped_refptr<File> file(new File);
    ASSERT_TRUE(file->Init(filename));
    EXPECT_TRUE(file->SetLength(15000));
  }

  // Initializing should fail, not crash.
  ASSERT_FALSE(files.Init(false));
}

// Tests detection of out of sync counters.
TEST_F(DiskCacheTest, BlockFiles_Counters) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  // Create a block of size 2.
  Addr address(0);
  EXPECT_TRUE(files.CreateBlock(RANKINGS, 2, &address));

  MappedFile* file = files.GetFile(address);
  ASSERT_TRUE(nullptr != file);

  BlockFileHeader* header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(nullptr != header);
  ASSERT_EQ(0, header->updating);

  // Alter the counters so that the free space doesn't add up.
  header->empty[2] = 50;  // 50 free blocks of size 3.
  files.CloseFiles();

  ASSERT_TRUE(files.Init(false));
  file = files.GetFile(address);
  ASSERT_TRUE(nullptr != file);
  header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(nullptr != header);

  // The file must have been fixed.
  ASSERT_EQ(0, header->empty[2]);

  // Change the number of entries.
  header->num_entries = 3;
  header->updating = 1;
  files.CloseFiles();

  ASSERT_TRUE(files.Init(false));
  file = files.GetFile(address);
  ASSERT_TRUE(nullptr != file);
  header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(nullptr != header);

  // The file must have been "fixed".
  ASSERT_EQ(2, header->num_entries);

  // Change the number of entries.
  header->num_entries = -1;
  header->updating = 1;
  files.CloseFiles();

  // Detect the error.
  ASSERT_FALSE(files.Init(false));
}

// An invalid file can be detected after init.
TEST_F(DiskCacheTest, BlockFiles_InvalidFile) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  // Let's access block 10 of file 5. (There is no file).
  Addr addr(BLOCK_256, 1, 5, 10);
  EXPECT_TRUE(nullptr == files.GetFile(addr));

  // Let's create an invalid file.
  base::FilePath filename(files.Name(5));
  char header[kBlockHeaderSize];
  memset(header, 'a', kBlockHeaderSize);
  EXPECT_EQ(kBlockHeaderSize,
            base::WriteFile(filename, header, kBlockHeaderSize));

  EXPECT_TRUE(nullptr == files.GetFile(addr));

  // The file should not have been changed (it is still invalid).
  EXPECT_TRUE(nullptr == files.GetFile(addr));
}

// Tests that we generate the correct file stats.
TEST_F(DiskCacheTest, BlockFiles_Stats) {
  ASSERT_TRUE(CopyTestCache("remove_load1"));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(false));
  int used, load;

  files.GetFileStats(0, &used, &load);
  EXPECT_EQ(101, used);
  EXPECT_EQ(9, load);

  files.GetFileStats(1, &used, &load);
  EXPECT_EQ(203, used);
  EXPECT_EQ(19, load);

  files.GetFileStats(2, &used, &load);
  EXPECT_EQ(0, used);
  EXPECT_EQ(0, load);
}

// Tests that we add and remove blocks correctly.
TEST_F(DiskCacheTest, AllocationMap) {
  ASSERT_TRUE(CleanupCacheDir());
  ASSERT_TRUE(base::CreateDirectory(cache_path_));

  BlockFiles files(cache_path_);
  ASSERT_TRUE(files.Init(true));

  // Create a bunch of entries.
  const int kSize = 100;
  Addr address[kSize];
  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    int block_size = i % 4 + 1;
    EXPECT_TRUE(files.CreateBlock(BLOCK_1K, block_size, &address[i]));
    EXPECT_EQ(BLOCK_1K, address[i].file_type());
    EXPECT_EQ(block_size, address[i].num_blocks());
    int start = address[i].start_block();
    EXPECT_EQ(start / 4, (start + block_size - 1) / 4);
  }

  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(files.IsValid(address[i]));
  }

  // The first part of the allocation map should be completely filled. We used
  // 10 bits per each four entries, so 250 bits total.
  BlockFileHeader* header =
      reinterpret_cast<BlockFileHeader*>(files.GetFile(address[0])->buffer());
  uint8_t* buffer = reinterpret_cast<uint8_t*>(&header->allocation_map);
  for (int i =0; i < 29; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(0xff, buffer[i]);
  }

  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    files.DeleteBlock(address[i], false);
  }

  // The allocation map should be empty.
  for (int i =0; i < 50; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(0, buffer[i]);
  }
}

}  // namespace disk_cache
