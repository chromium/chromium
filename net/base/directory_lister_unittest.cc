// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

const int kMaxDepth = 3;
const int kBranchingFactor = 4;
const int kFilesPerDirectory = 5;

class ListerDelegate : public DirectoryLister::DirectoryListerDelegate {
 public:
  explicit ListerDelegate(DirectoryLister::ListingType type) : type_(type) {}

  // When set to true, this signals that the directory list operation should be
  // cancelled (And the run loop quit) in the first call to OnListFile.
  void set_cancel_lister_on_list_file(bool cancel_lister_on_list_file) {
    cancel_lister_on_list_file_ = cancel_lister_on_list_file;
  }

  // When set to true, this signals that the directory list operation should be
  // cancelled (And the run loop quit) when OnDone is called.
  void set_cancel_lister_on_list_done(bool cancel_lister_on_list_done) {
    cancel_lister_on_list_done_ = cancel_lister_on_list_done;
  }

  void OnListFile(const DirectoryLister::DirectoryListerData& data) override {
    ASSERT_FALSE(done_);

    file_list_.push_back(data.info);
    paths_.push_back(data.path);
    if (cancel_lister_on_list_file_) {
      lister_->Cancel();
      run_loop.Quit();
    }
  }

  void OnListDone(int error) override {
    ASSERT_FALSE(done_);

    done_ = true;
    error_ = error;
    if (type_ == DirectoryLister::ALPHA_DIRS_FIRST)
      CheckSort();

    if (cancel_lister_on_list_done_)
      lister_->Cancel();
    run_loop.Quit();
  }

  void CheckSort() {
    // Check that we got files in the right order.
    if (!file_list_.empty()) {
      for (size_t previous = 0, current = 1;
           current < file_list_.size();
           previous++, current++) {
        // Directories should come before files.
        if (file_list_[previous].IsDirectory() &&
            !file_list_[current].IsDirectory()) {
          continue;
        }
        EXPECT_NE(FILE_PATH_LITERAL(".."),
                  file_list_[current].GetName().BaseName().value());
        EXPECT_EQ(file_list_[previous].IsDirectory(),
                  file_list_[current].IsDirectory());
        EXPECT_TRUE(base::i18n::LocaleAwareCompareFilenames(
            file_list_[previous].GetName(),
            file_list_[current].GetName()));
      }
    }
  }

  void Run(DirectoryLister* lister) {
    lister_ = lister;
    lister_->Start();
    run_loop.Run();
  }

  int error() const { return error_; }

  int num_files() const { return file_list_.size(); }

  bool done() const { return done_; }

 private:
  bool cancel_lister_on_list_file_ = false;
  bool cancel_lister_on_list_done_ = false;

  // This is owned by the individual tests, rather than the ListerDelegate.
  raw_ptr<DirectoryLister> lister_ = nullptr;

  base::RunLoop run_loop;

  bool done_ = false;
  int error_ = -1;
  DirectoryLister::ListingType type_;

  std::vector<base::FileEnumerator::FileInfo> file_list_;
  std::vector<base::FilePath> paths_;
};

}  // namespace

class DirectoryListerTest : public PlatformTest, public WithTaskEnvironment {
 public:
  DirectoryListerTest() = default;

  void SetUp() override {
    // Randomly create a directory structure of depth 3 in a temporary root
    // directory.
    std::list<std::pair<base::FilePath, int> > directories;
    ASSERT_TRUE(temp_root_dir_.CreateUniqueTempDir());
    directories.emplace_back(temp_root_dir_.GetPath(), 0);
    while (!directories.empty()) {
      std::pair<base::FilePath, int> dir_data = directories.front();
      directories.pop_front();
      for (int i = 0; i < kFilesPerDirectory; i++) {
        std::string file_name = base::StringPrintf("file_id_%d", i);
        base::FilePath file_path = dir_data.first.AppendASCII(file_name);
        base::File file(file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
        ASSERT_TRUE(file.IsValid());
        ++total_created_file_system_objects_in_temp_root_dir_;
        if (dir_data.first == temp_root_dir_.GetPath())
          ++created_file_system_objects_in_temp_root_dir_;
      }
      if (dir_data.second < kMaxDepth - 1) {
        for (int i = 0; i < kBranchingFactor; i++) {
          std::string dir_name = base::StringPrintf("child_dir_%d", i);
          base::FilePath dir_path = dir_data.first.AppendASCII(dir_name);
          ASSERT_TRUE(base::CreateDirectory(dir_path));
          ++total_created_file_system_objects_in_temp_root_dir_;
          if (dir_data.first == temp_root_dir_.GetPath())
            ++created_file_system_objects_in_temp_root_dir_;
          directories.emplace_back(dir_path, dir_data.second + 1);
        }
      }
    }
    PlatformTest::SetUp();
  }

  const base::FilePath& root_path() const { return temp_root_dir_.GetPath(); }

  int expected_list_length_recursive() const {
    // List should include everything but the top level directory, and does not
    // include "..".
    return total_created_file_system_objects_in_temp_root_dir_;
  }

  int expected_list_length_non_recursive() const {
    // List should include everything in the top level directory, and "..".
    return created_file_system_objects_in_temp_root_dir_ + 1;
  }

 private:
  // Number of files and directories created in SetUp, excluding
  // |temp_root_dir_| itself.  Includes all nested directories and their files.
  int total_created_file_system_objects_in_temp_root_dir_ = 0;
  // Number of files and directories created directly in |temp_root_dir_|.
  int created_file_system_objects_in_temp_root_dir_ = 0;

  base::ScopedTempDir temp_root_dir_;
};

TEST_F(DirectoryListerTest, BigDirTest) {
  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(root_path(), &delegate);
  delegate.Run(&lister);

  EXPECT_TRUE(delegate.done());
  EXPECT_THAT(delegate.error(), IsOk());
  EXPECT_EQ(expected_list_length_non_recursive(), delegate.num_files());
}

TEST_F(DirectoryListerTest, BigDirRecursiveTest) {
  ListerDelegate delegate(DirectoryLister::NO_SORT_RECURSIVE);
  DirectoryLister lister(root_path(), DirectoryLister::NO_SORT_RECURSIVE,
                         &delegate);
  delegate.Run(&lister);

  EXPECT_TRUE(delegate.done());
  EXPECT_THAT(delegate.error(), IsOk());
  EXPECT_EQ(expected_list_length_recursive(), delegate.num_files());
}

TEST_F(DirectoryListerTest, EmptyDirTest) {
  base::ScopedTempDir tempDir;
  EXPECT_TRUE(tempDir.CreateUniqueTempDir());

  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(tempDir.GetPath(), &delegate);
  delegate.Run(&lister);

  EXPECT_TRUE(delegate.done());
  EXPECT_THAT(delegate.error(), IsOk());
  // Contains only the parent directory ("..").
  EXPECT_EQ(1, delegate.num_files());
}

// This doesn't really test much, except make sure calling cancel before any
// callbacks are invoked doesn't crash.  Can't wait for all tasks running on a
// worker pool to complete, unfortunately.
// TODO(mmenke):  See if there's a way to make this fail more reliably on
// regression.
TEST_F(DirectoryListerTest, BasicCancelTest) {
  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  auto lister = std::make_unique<DirectoryLister>(root_path(), &delegate);
  lister->Start();
  lister->Cancel();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate.done());
  EXPECT_EQ(0, delegate.num_files());
}

TEST_F(DirectoryListerTest, CancelOnListFileTest) {
  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(root_path(), &delegate);
  delegate.set_cancel_lister_on_list_file(true);
  delegate.Run(&lister);

  EXPECT_FALSE(delegate.done());
  EXPECT_EQ(1, delegate.num_files());
}

TEST_F(DirectoryListerTest, CancelOnListDoneTest) {
  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(root_path(), &delegate);
  delegate.set_cancel_lister_on_list_done(true);
  delegate.Run(&lister);

  EXPECT_TRUE(delegate.done());
  EXPECT_THAT(delegate.error(), IsOk());
  EXPECT_EQ(expected_list_length_non_recursive(), delegate.num_files());
}

TEST_F(DirectoryListerTest, CancelOnLastElementTest) {
  base::ScopedTempDir tempDir;
  EXPECT_TRUE(tempDir.CreateUniqueTempDir());

  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(tempDir.GetPath(), &delegate);
  delegate.set_cancel_lister_on_list_file(true);
  delegate.Run(&lister);

  EXPECT_FALSE(delegate.done());
  // Contains only the parent directory ("..").
  EXPECT_EQ(1, delegate.num_files());
}

TEST_F(DirectoryListerTest, NoSuchDirTest) {
  base::ScopedTempDir tempDir;
  EXPECT_TRUE(tempDir.CreateUniqueTempDir());

  ListerDelegate delegate(DirectoryLister::ALPHA_DIRS_FIRST);
  DirectoryLister lister(
      tempDir.GetPath().AppendASCII("this_path_does_not_exist"), &delegate);
  delegate.Run(&lister);

  EXPECT_THAT(delegate.error(), IsError(ERR_FILE_NOT_FOUND));
  EXPECT_EQ(0, delegate.num_files());
}

}  // namespace net
