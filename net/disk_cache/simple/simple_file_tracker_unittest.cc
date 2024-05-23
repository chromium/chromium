// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/cache_type.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class SimpleFileTrackerTest : public DiskCacheTest {
 public:
  void DeleteSyncEntry(SimpleSynchronousEntry* entry) { delete entry; }

  // We limit open files to 4 for the fixture, as this is large enough
  // that simple tests don't have to worry about naming files normally,
  // but small enough to test with easily.
  static const int kFileLimit = 4;

 protected:
  SimpleFileTrackerTest() : file_tracker_(kFileLimit) {}

  // A bit of messiness since we rely on friendship of the fixture to be able to
  // create/delete SimpleSynchronousEntry objects.
  class SyncEntryDeleter {
   public:
    explicit SyncEntryDeleter(SimpleFileTrackerTest* fixture)
        : fixture_(fixture) {}
    void operator()(SimpleSynchronousEntry* entry) {
      fixture_->DeleteSyncEntry(entry);
    }

   private:
    raw_ptr<SimpleFileTrackerTest> fixture_;
  };

  using SyncEntryPointer =
      std::unique_ptr<SimpleSynchronousEntry, SyncEntryDeleter>;

  SyncEntryPointer MakeSyncEntry(uint64_t hash) {
    return SyncEntryPointer(
        new SimpleSynchronousEntry(
            net::DISK_CACHE, cache_path_, "dummy", hash, &file_tracker_,
            base::MakeRefCounted<disk_cache::TrivialFileOperationsFactory>()
                ->CreateUnbound(),
            /*stream_0_size=*/-1),
        SyncEntryDeleter(this));
  }

  void UpdateEntryFileKey(SimpleSynchronousEntry* sync_entry,
                          SimpleFileTracker::EntryFileKey file_key) {
    sync_entry->entry_file_key_ = file_key;
  }

  SimpleFileTracker file_tracker_;
};

TEST_F(SimpleFileTrackerTest, Basic) {
  SyncEntryPointer entry = MakeSyncEntry(1);
  TrivialFileOperations ops;

  // Just transfer some files to the tracker, and then do some I/O on getting
  // them back.
  base::FilePath path_0 = cache_path_.AppendASCII("file_0");
  base::FilePath path_1 = cache_path_.AppendASCII("file_1");

  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      path_0, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file_1 = std::make_unique<base::File>(
      path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  ASSERT_TRUE(file_1->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1));

  std::string_view msg_0 = "Hello";
  std::string_view msg_1 = "Worldish Place";

  {
    SimpleFileTracker::FileHandle borrow_0 = file_tracker_.Acquire(
        &ops, entry.get(), SimpleFileTracker::SubFile::FILE_0);
    SimpleFileTracker::FileHandle borrow_1 = file_tracker_.Acquire(
        &ops, entry.get(), SimpleFileTracker::SubFile::FILE_1);

    EXPECT_EQ(static_cast<int>(msg_0.size()),
              borrow_0->Write(0, msg_0.data(), msg_0.size()));
    EXPECT_EQ(static_cast<int>(msg_1.size()),
              borrow_1->Write(0, msg_1.data(), msg_1.size()));

    // For stream 0 do release/close, for stream 1 do close/release --- where
    // release happens when borrow_{0,1} go out of scope
    file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  }
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify contents.
  std::string verify_0, verify_1;
  EXPECT_TRUE(ReadFileToString(path_0, &verify_0));
  EXPECT_TRUE(ReadFileToString(path_1, &verify_1));
  EXPECT_EQ(msg_0, verify_0);
  EXPECT_EQ(msg_1, verify_1);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, Collision) {
  // Two entries with same key.
  SyncEntryPointer entry = MakeSyncEntry(1);
  SyncEntryPointer entry2 = MakeSyncEntry(1);
  TrivialFileOperations ops;

  base::FilePath path = cache_path_.AppendASCII("file");
  base::FilePath path2 = cache_path_.AppendASCII("file2");

  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file2 = std::make_unique<base::File>(
      path2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file->IsValid());
  ASSERT_TRUE(file2->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file));
  file_tracker_.Register(entry2.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file2));

  std::string_view msg = "Alpha";
  std::string_view msg2 = "Beta";

  {
    SimpleFileTracker::FileHandle borrow = file_tracker_.Acquire(
        &ops, entry.get(), SimpleFileTracker::SubFile::FILE_0);
    SimpleFileTracker::FileHandle borrow2 = file_tracker_.Acquire(
        &ops, entry2.get(), SimpleFileTracker::SubFile::FILE_0);

    EXPECT_EQ(static_cast<int>(msg.size()),
              borrow->Write(0, msg.data(), msg.size()));
    EXPECT_EQ(static_cast<int>(msg2.size()),
              borrow2->Write(0, msg2.data(), msg2.size()));
  }
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);
  file_tracker_.Close(entry2.get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify contents.
  std::string verify, verify2;
  EXPECT_TRUE(ReadFileToString(path, &verify));
  EXPECT_TRUE(ReadFileToString(path2, &verify2));
  EXPECT_EQ(msg, verify);
  EXPECT_EQ(msg2, verify2);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, Reopen) {
  // We may sometimes go Register -> Close -> Register, with info still
  // alive.
  SyncEntryPointer entry = MakeSyncEntry(1);

  base::FilePath path_0 = cache_path_.AppendASCII("file_0");
  base::FilePath path_1 = cache_path_.AppendASCII("file_1");

  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      path_0, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  std::unique_ptr<base::File> file_1 = std::make_unique<base::File>(
      path_1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  ASSERT_TRUE(file_1->IsValid());

  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1));

  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  std::unique_ptr<base::File> file_1b = std::make_unique<base::File>(
      path_1, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_1b->IsValid());
  file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_1,
                         std::move(file_1b));
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);
  file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_1);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, PointerStability) {
  // Make sure the FileHandle lent out doesn't get screwed up as we update
  // the state (and potentially move the underlying base::File object around).
  const int kEntries = 8;
  SyncEntryPointer entries[kEntries] = {
      MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1),
      MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1), MakeSyncEntry(1),
  };
  TrivialFileOperations ops;
  std::unique_ptr<base::File> file_0 = std::make_unique<base::File>(
      cache_path_.AppendASCII("0"),
      base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file_0->IsValid());
  file_tracker_.Register(entries[0].get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file_0));

  std::string_view msg = "Message to write";
  {
    SimpleFileTracker::FileHandle borrow = file_tracker_.Acquire(
        &ops, entries[0].get(), SimpleFileTracker::SubFile::FILE_0);
    for (int i = 1; i < kEntries; ++i) {
      std::unique_ptr<base::File> file_n = std::make_unique<base::File>(
          cache_path_.AppendASCII(base::NumberToString(i)),
          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      ASSERT_TRUE(file_n->IsValid());
      file_tracker_.Register(entries[i].get(),
                             SimpleFileTracker::SubFile::FILE_0,
                             std::move(file_n));
    }

    EXPECT_EQ(static_cast<int>(msg.size()),
              borrow->Write(0, msg.data(), msg.size()));
  }

  for (const auto& entry : entries)
    file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);

  // Verify the file.
  std::string verify;
  EXPECT_TRUE(ReadFileToString(cache_path_.AppendASCII("0"), &verify));
  EXPECT_EQ(msg, verify);
  EXPECT_TRUE(file_tracker_.IsEmptyForTesting());
}

TEST_F(SimpleFileTrackerTest, Doom) {
  SyncEntryPointer entry1 = MakeSyncEntry(1);
  base::FilePath path1 = cache_path_.AppendASCII("file1");
  std::unique_ptr<base::File> file1 = std::make_unique<base::File>(
      path1, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file1->IsValid());

  file_tracker_.Register(entry1.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file1));
  SimpleFileTracker::EntryFileKey key1 = entry1->entry_file_key();
  file_tracker_.Doom(entry1.get(), &key1);
  EXPECT_NE(0u, key1.doom_generation);

  // Other entry with same key.
  SyncEntryPointer entry2 = MakeSyncEntry(1);
  base::FilePath path2 = cache_path_.AppendASCII("file2");
  std::unique_ptr<base::File> file2 = std::make_unique<base::File>(
      path2, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(file2->IsValid());

  file_tracker_.Register(entry2.get(), SimpleFileTracker::SubFile::FILE_0,
                         std::move(file2));
  SimpleFileTracker::EntryFileKey key2 = entry2->entry_file_key();
  file_tracker_.Doom(entry2.get(), &key2);
  EXPECT_NE(0u, key2.doom_generation);
  EXPECT_NE(key1.doom_generation, key2.doom_generation);

  file_tracker_.Close(entry1.get(), SimpleFileTracker::SubFile::FILE_0);
  file_tracker_.Close(entry2.get(), SimpleFileTracker::SubFile::FILE_0);
}

TEST_F(SimpleFileTrackerTest, OverLimit) {
  base::HistogramTester histogram_tester;

  const int kEntries = 10;  // want more than FD limit in fixture.
  std::vector<SyncEntryPointer> entries;
  std::vector<base::FilePath> names;
  TrivialFileOperations ops;
  for (int i = 0; i < kEntries; ++i) {
    SyncEntryPointer entry = MakeSyncEntry(i);
    base::FilePath name =
        entry->GetFilenameForSubfile(SimpleFileTracker::SubFile::FILE_0);
    std::unique_ptr<base::File> file = std::make_unique<base::File>(
        name, base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                  base::File::FLAG_READ);
    ASSERT_TRUE(file->IsValid());
    file_tracker_.Register(entry.get(), SimpleFileTracker::SubFile::FILE_0,
                           std::move(file));
    entries.push_back(std::move(entry));
    names.push_back(name);
  }

  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_CLOSE_FILE,
                                     kEntries - kFileLimit);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE, 0);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);

  // Grab the last one; we will hold it open till the end of the test. It's
  // still open, so no change in stats after.
  SimpleFileTracker::FileHandle borrow_last = file_tracker_.Acquire(
      &ops, entries[kEntries - 1].get(), SimpleFileTracker::SubFile::FILE_0);
  EXPECT_EQ(1, borrow_last->Write(0, "L", 1));

  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_CLOSE_FILE,
                                     kEntries - kFileLimit);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE, 0);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 0);

  // Delete file for [2], to cause error on its re-open.
  EXPECT_TRUE(base::DeleteFile(names[2])) << names[2];

  // Reacquire all the other files.
  for (int i = 0; i < kEntries - 1; ++i) {
    SimpleFileTracker::FileHandle borrow = file_tracker_.Acquire(
        &ops, entries[i].get(), SimpleFileTracker::SubFile::FILE_0);
    if (i != 2) {
      EXPECT_TRUE(borrow.IsOK());
      char c = static_cast<char>(i);
      EXPECT_EQ(1, borrow->Write(0, &c, 1));
    } else {
      EXPECT_FALSE(borrow.IsOK());
    }
  }

  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_CLOSE_FILE,
                                     kEntries - kFileLimit + kEntries - 2);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE,
                                     kEntries - 2);
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 1);

  // Doom file for [1].
  SimpleFileTracker::EntryFileKey key = entries[1]->entry_file_key();
  file_tracker_.Doom(entries[1].get(), &key);
  base::FilePath old_path = names[1];
  UpdateEntryFileKey(entries[1].get(), key);
  base::FilePath new_path =
      entries[1]->GetFilenameForSubfile(SimpleFileTracker::SubFile::FILE_0);
  EXPECT_TRUE(new_path.BaseName().MaybeAsASCII().starts_with("todelete_"));
  EXPECT_TRUE(base::Move(old_path, new_path));

  // Now re-acquire everything again; this time reading.
  for (int i = 0; i < kEntries - 1; ++i) {
    SimpleFileTracker::FileHandle borrow = file_tracker_.Acquire(
        &ops, entries[i].get(), SimpleFileTracker::SubFile::FILE_0);
    char read;
    char expected = static_cast<char>(i);
    if (i != 2) {
      EXPECT_TRUE(borrow.IsOK());
      EXPECT_EQ(1, borrow->Read(0, &read, 1));
      EXPECT_EQ(expected, read);
    } else {
      EXPECT_FALSE(borrow.IsOK());
    }
  }

  histogram_tester.ExpectBucketCount(
      "SimpleCache.FileDescriptorLimiterAction",
      disk_cache::FD_LIMIT_CLOSE_FILE,
      kEntries - kFileLimit + 2 * (kEntries - 2));
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_REOPEN_FILE,
                                     2 * (kEntries - 2));
  histogram_tester.ExpectBucketCount("SimpleCache.FileDescriptorLimiterAction",
                                     disk_cache::FD_LIMIT_FAIL_REOPEN_FILE, 2);

  // Read from the last one, too. Should still be fine.
  char read;
  EXPECT_EQ(1, borrow_last->Read(0, &read, 1));
  EXPECT_EQ('L', read);

  for (const auto& entry : entries)
    file_tracker_.Close(entry.get(), SimpleFileTracker::SubFile::FILE_0);
}

}  // namespace disk_cache
