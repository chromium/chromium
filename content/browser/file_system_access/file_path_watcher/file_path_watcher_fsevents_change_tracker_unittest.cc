// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_fsevents_change_tracker.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using ChangeInfo = FilePathWatcher::ChangeInfo;
using FilePathType = FilePathWatcher::FilePathType;
using ChangeType = FilePathWatcher::ChangeType;

struct Event {
  ChangeInfo change_info;
  base::FilePath path;
  bool error;

  bool operator==(const Event& other) const {
    return error == other.error && path == other.path &&
           change_info == other.change_info;
  }
};

Event CreatedFile(base::FilePath path) {
  return {ChangeInfo(FilePathType::kFile, ChangeType::kCreated, path), path,
          /*error=*/false};
}

Event DeletedFile(base::FilePath path) {
  return {ChangeInfo(FilePathType::kFile, ChangeType::kDeleted, path), path,
          /*error=*/false};
}

Event ModifiedFile(base::FilePath path) {
  return {ChangeInfo(FilePathType::kFile, ChangeType::kModified, path), path,
          /*error=*/false};
}

Event MovedFile(base::FilePath from_path, base::FilePath to_path) {
  return {
      ChangeInfo(FilePathType::kFile, ChangeType::kMoved, to_path, from_path),
      to_path,
      /*error=*/false};
}

Event CreatedDirectory(base::FilePath path) {
  return {ChangeInfo(FilePathType::kDirectory, ChangeType::kCreated, path),
          path,
          /*error=*/false};
}

Event DeletedDirectory(base::FilePath path) {
  return {ChangeInfo(FilePathType::kDirectory, ChangeType::kDeleted, path),
          path,
          /*error=*/false};
}

Event MovedDirectory(base::FilePath from_path, base::FilePath to_path) {
  return {ChangeInfo(FilePathType::kDirectory, ChangeType::kMoved, to_path,
                     from_path),
          to_path,
          /*error=*/false};
}

}  // namespace

class FilePathWatcherFSEventsChangeTrackerTest : public testing::Test {
 public:
  FilePathWatcherFSEventsChangeTrackerTest() = default;

  void SetUp() override {
    // Temporary files in Mac are created under /var/, which is a symlink that
    // resolves to /private/var/. Set `temp_dir_` directly to the resolved file
    // path, given that the expected FSEvents event paths are reported as
    // resolved paths.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath resolved_path =
        base::MakeAbsoluteFilePath(temp_dir_.GetPath());
    if (!resolved_path.empty()) {
      temp_dir_.Take();
      ASSERT_TRUE(temp_dir_.Set(resolved_path));
    }

    change_tracker_ = FilePathWatcherFSEventsChangeTracker{
        base::BindRepeating(&FilePathWatcherFSEventsChangeTrackerTest::Callback,
                            base::Unretained(this)),
        temp_dir_.GetPath(), FilePathWatcher::Type::kRecursive,
        /*report_modified_path=*/true};
  }

  void DispatchEvents(
      std::map<FSEventStreamEventId,
               FilePathWatcherFSEventsChangeTracker::ChangeEvent> events) {
    change_tracker_->DispatchEvents(std::move(events));
  }

  void Callback(const ChangeInfo& change_info,
                const base::FilePath& path,
                bool error) {
    file_path_watcher_events_.emplace_back(change_info, path, error);
  }

  uint64_t CreateFile(base::FilePath path) {
    const char* path_cstr = path.value().c_str();
    int fd = open(path_cstr, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR);

    CHECK_GE(fd, 0);

    return GetInodeForFileDescriptor(fd);
  }

  uint64_t CreateDirectory(base::FilePath path) {
    const char* path_cstr = path.value().c_str();
    mkdir(path_cstr, S_IRUSR | S_IWUSR);

    int fd = open(path_cstr, O_RDONLY);
    CHECK_GE(fd, 0);

    return GetInodeForFileDescriptor(fd);
  }

  uint64_t GetInodeForFileDescriptor(int fd) {
    struct stat fd_stat;
    CHECK_GE(fstat(fd, &fd_stat), 0);

    return fd_stat.st_ino;
  }

 protected:
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::optional<FilePathWatcherFSEventsChangeTracker> change_tracker_;

  std::vector<Event> file_path_watcher_events_;
};

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, CreateFile) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t file_inode = CreateFile(a_path);

  DispatchEvents({
      {153839757,
       {
           kFSEventStreamEventFlagItemCreated |
               kFSEventStreamEventFlagItemIsFile,
           a_path,
           file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(CreatedFile(a_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, DeleteFile) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t file_inode = 84699405;

  DispatchEvents({
      {156235806,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRemoved,
           a_path,
           file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(DeletedFile(a_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, ModifyFile) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t file_inode = CreateFile(a_path);

  DispatchEvents({
      {156296102,
       {
           kFSEventStreamEventFlagItemInodeMetaMod |
               kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemModified,
           a_path,
           file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(ModifiedFile(a_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, MoveFile) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  base::FilePath b_path = temp_dir_.GetPath().AppendASCII("b");
  uint64_t file_inode = CreateFile(b_path);

  DispatchEvents({
      {156251254,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           a_path,
           file_inode,
       }},
      {156251255,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(MovedFile(a_path, b_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, MoveWithOverwriteFile) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  base::FilePath b_path = temp_dir_.GetPath().AppendASCII("b");
  uint64_t old_file_inode = 12345;
  uint64_t file_inode = CreateFile(b_path);

  DispatchEvents({
      {156251254,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           a_path,
           file_inode,
       }},
      {156251256,  // FSEvents generates additional rename event for overwrite
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           old_file_inode,
       }},
      {156251259,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(MovedFile(a_path, b_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest,
       MoveWithOverwriteFileOnDifferentOrder) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  base::FilePath b_path = temp_dir_.GetPath().AppendASCII("b");
  uint64_t old_file_inode = 12345;
  uint64_t file_inode = CreateFile(b_path);

  DispatchEvents({
      {156251254,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           a_path,
           file_inode,
       }},
      {156251256,
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           file_inode,
       }},
      {156251259,  // FSEvents generates additional rename event for overwrite
       {
           kFSEventStreamEventFlagItemIsFile |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           old_file_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(MovedFile(a_path, b_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, CreateDirectory) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t dir_inode = CreateDirectory(a_path);

  DispatchEvents({
      {156332709,
       {
           kFSEventStreamEventFlagItemCreated |
               kFSEventStreamEventFlagItemIsDir,
           a_path,
           dir_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(CreatedDirectory(a_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, DeleteDirectory) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t dir_inode = 84860544;

  DispatchEvents({
      {156334324,
       {
           kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRemoved,
           a_path,
           dir_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(DeletedDirectory(a_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, ModifyDirectory) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  uint64_t dir_inode = CreateDirectory(a_path);

  DispatchEvents({
      {156363861,
       {
           kFSEventStreamEventFlagItemInodeMetaMod |
               kFSEventStreamEventFlagItemIsDir,
           a_path,
           dir_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_, testing::IsEmpty());
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, MoveDirectory) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  base::FilePath b_path = temp_dir_.GetPath().AppendASCII("b");
  uint64_t dir_inode = CreateDirectory(b_path);

  DispatchEvents({
      {156348258,
       {
           kFSEventStreamEventFlagItemCreated |
               kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRenamed,
           a_path,
           dir_inode,
       }},
      {156348259,
       {
           kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           dir_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(MovedDirectory(a_path, b_path)));
}

TEST_F(FilePathWatcherFSEventsChangeTrackerTest, MoveWithOverwriteDirectory) {
  base::FilePath a_path = temp_dir_.GetPath().AppendASCII("a");
  base::FilePath b_path = temp_dir_.GetPath().AppendASCII("b");
  uint64_t old_dir_inode = 12345;
  uint64_t dir_inode = CreateDirectory(b_path);

  DispatchEvents({
      {156251254,
       {
           kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRenamed,
           a_path,
           dir_inode,
       }},
      {156251256,  // FSEvents generates additional rename event for overwrite
       {
           kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           old_dir_inode,
       }},
      {156251259,
       {
           kFSEventStreamEventFlagItemIsDir |
               kFSEventStreamEventFlagItemRenamed,
           b_path,
           dir_inode,
       }},
  });

  EXPECT_THAT(file_path_watcher_events_,
              testing::ElementsAre(MovedDirectory(a_path, b_path)));
}

}  // namespace content
