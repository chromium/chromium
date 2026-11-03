// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/created_session_tracker.h"

#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using Entry = CreatedSessionTracker::Entry;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

// Returns the permission bits of the regular file at `path`.
mode_t GetRegularFileMode(const base::FilePath& path) {
  struct stat stat_buf = {};
  EXPECT_EQ(lstat(path.value().c_str(), &stat_buf), 0);
  // Make sure that masking the mode below doesn't hide e.g. a symlink.
  EXPECT_TRUE(S_ISREG(stat_buf.st_mode));
  return stat_buf.st_mode & 07777;
}

}  // namespace

class CreatedSessionTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    state_dir_ = temp_dir_.GetPath().Append("chrome-remote-desktop");
    state_file_ = state_dir_.Append("created_sessions.json");
    current_uid_ = geteuid();
    // In production, the directory is created by the daemon process.
    ASSERT_TRUE(base::CreateDirectory(state_dir_));
    ASSERT_TRUE(base::SetPosixFilePermissions(state_dir_, 0700));
  }

 protected:
  CreatedSessionTracker CreateTracker() {
    return CreatedSessionTracker(state_file_, current_uid_);
  }

  void WriteStateFile(const std::string& content, int file_mode = 0600) {
    ASSERT_TRUE(base::WriteFile(state_file_, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(state_file_, file_mode));
  }

  std::string ReadStateFile() {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(state_file_, &content));
    return content;
  }

  // Replaces the state directory with a symlink to a new directory, and
  // returns the new directory.
  base::FilePath ReplaceStateDirectoryWithSymlink() {
    base::FilePath real_dir = temp_dir_.GetPath().Append("real");
    EXPECT_TRUE(base::DeletePathRecursively(state_dir_));
    EXPECT_TRUE(base::CreateDirectory(real_dir));
    EXPECT_TRUE(base::CreateSymbolicLink(real_dir, state_dir_));
    return real_dir;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath state_dir_;
  base::FilePath state_file_;
  uid_t current_uid_ = 0;
};

TEST_F(CreatedSessionTrackerTest, DefaultFilePath) {
  EXPECT_EQ(CreatedSessionTracker::GetDefaultFilePath().value(),
            "/run/chrome-remote-desktop/created_sessions.json");
}

TEST_F(CreatedSessionTrackerTest, LoadReturnsEmptyIfDirectoryIsMissing) {
  ASSERT_TRUE(base::DeletePathRecursively(state_dir_));
  auto entries = CreateTracker().Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, IsEmpty());
}

TEST_F(CreatedSessionTrackerTest, LoadReturnsEmptyIfFileIsMissing) {
  auto entries = CreateTracker().Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, IsEmpty());
}

TEST_F(CreatedSessionTrackerTest, AddThenLoad) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"3", "alice", 1000}).has_value());
  ASSERT_TRUE(tracker.Add({"c2", "bob", 1001}).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries,
              ElementsAre(Entry{"3", "alice", 1000}, Entry{"c2", "bob", 1001}));
}

TEST_F(CreatedSessionTrackerTest, EntriesPersistAcrossInstances) {
  ASSERT_TRUE(CreateTracker().Add({"3", "alice", 1000}).has_value());

  auto entries = CreateTracker().Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"3", "alice", 1000}));
}

TEST_F(CreatedSessionTrackerTest, AddWritesExpectedFormat) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"3", "alice", 1000}).has_value());
  ASSERT_TRUE(tracker.Add({"c2", "bob", 4000000000u}).has_value());

  // UIDs are written as plain integers, even when they exceed INT_MAX.
  EXPECT_EQ(ReadStateFile(), R"({"3":{"uid":1000,"username":"alice"},)"
                             R"("c2":{"uid":4000000000,"username":"bob"}})");
  EXPECT_EQ(GetRegularFileMode(state_file_), mode_t{0600});
}

TEST_F(CreatedSessionTrackerTest, AddReplacesEntryWithSameSessionId) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"3", "alice", 1000}).has_value());
  ASSERT_TRUE(tracker.Add({"3", "bob", 1001}).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"3", "bob", 1001}));
}

TEST_F(CreatedSessionTrackerTest, AddSupportsUidsAboveInt32Max) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"3", "alice", 4294967294u}).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"3", "alice", 4294967294u}));
}

TEST_F(CreatedSessionTrackerTest, AddRejectsInvalidEntries) {
  const Entry kInvalidEntries[] = {
      {"", "alice", 1000},
      {"3/../4", "alice", 1000},
      {std::string(65, '1'), "alice", 1000},
      {"3", "", 1000},
      {"3", "-alice", 1000},
      {"3", "al ice", 1000},
      {"3", std::string(257, 'a'), 1000},
      {"3", "alice", static_cast<uid_t>(-1)},
  };
  auto tracker = CreateTracker();
  for (const Entry& entry : kInvalidEntries) {
    SCOPED_TRACE(base::StrCat({entry.session_id, " ", entry.username, " ",
                               base::NumberToString(entry.uid)}));
    EXPECT_FALSE(tracker.Add(entry).has_value());
  }

  EXPECT_FALSE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, AddAcceptsMaxLengthFields) {
  const Entry kEntry{std::string(64, '1'), std::string(256, 'a'), 1000};
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add(kEntry).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(kEntry));
}

TEST_F(CreatedSessionTrackerTest, AddFailsWithoutChangesIfFileWouldBeTooLarge) {
  // Write a state file that is as large as possible without exceeding the
  // 64 KiB limit, using the same compact JSON format and field lengths as the
  // entry added below, so that adding it is guaranteed to exceed the limit.
  // Must match kMaxStateFileSizeBytes in created_session_tracker.cc.
  constexpr size_t kMaxStateFileSizeBytes = 64 * 1024;
  const std::string kUsername(256, 'a');
  auto entry_json = [&kUsername](int index) {
    return base::StringPrintf(R"("%04d":{"uid":1000,"username":"%s"})", index,
                              kUsername.c_str());
  };
  std::string sessions;
  int entry_count = 0;
  while (true) {
    std::string next = (entry_count > 0 ? "," : "") + entry_json(entry_count);
    // +2 for the enclosing braces.
    if (sessions.size() + next.size() + 2 > kMaxStateFileSizeBytes) {
      break;
    }
    sessions += next;
    ++entry_count;
  }
  WriteStateFile("{" + sessions + "}");

  auto tracker = CreateTracker();
  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  ASSERT_EQ(entries->size(), static_cast<size_t>(entry_count));

  EXPECT_FALSE(tracker.Add({"9999", kUsername, 1000}).has_value());

  // The existing file must still be readable and updatable.
  entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_EQ(entries->size(), static_cast<size_t>(entry_count));

  std::vector<std::string> to_remove = {"0000"};
  ASSERT_TRUE(tracker.Remove(to_remove).has_value());
  entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_EQ(entries->size(), static_cast<size_t>(entry_count - 1));
}

TEST_F(CreatedSessionTrackerTest, AddReplacesMalformedFile) {
  WriteStateFile("not json");

  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"3", "alice", 1000}).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"3", "alice", 1000}));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfDirectoryIsMissing) {
  ASSERT_TRUE(base::DeletePathRecursively(state_dir_));

  EXPECT_FALSE(CreateTracker().Add({"3", "alice", 1000}).has_value());
  EXPECT_FALSE(base::PathExists(state_dir_));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfDirectoryIsSymlink) {
  base::FilePath real_dir = ReplaceStateDirectoryWithSymlink();

  EXPECT_FALSE(CreateTracker().Add({"3", "alice", 1000}).has_value());
  EXPECT_FALSE(base::PathExists(real_dir.Append("created_sessions.json")));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfDirectoryHasWrongOwner) {
  CreatedSessionTracker tracker(state_file_, current_uid_ + 1);
  EXPECT_FALSE(tracker.Add({"3", "alice", 1000}).has_value());
  EXPECT_FALSE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfDirectoryIsWritableByOthers) {
  ASSERT_TRUE(base::SetPosixFilePermissions(state_dir_, 0777));

  EXPECT_FALSE(CreateTracker().Add({"3", "alice", 1000}).has_value());
  EXPECT_FALSE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfStateFileIsSymlink) {
  constexpr char kTargetContent[] = "unchanged";
  base::FilePath target = temp_dir_.GetPath().Append("target.json");
  ASSERT_TRUE(base::WriteFile(target, kTargetContent));
  ASSERT_TRUE(base::CreateSymbolicLink(target, state_file_));

  EXPECT_FALSE(CreateTracker().Add({"3", "alice", 1000}).has_value());

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(target, &content));
  EXPECT_EQ(content, kTargetContent);
  EXPECT_TRUE(base::IsLink(state_file_));
}

TEST_F(CreatedSessionTrackerTest, AddFailsIfStateFileIsGroupWritable) {
  constexpr char kContent[] = R"({"1": {"username": "bob", "uid": 1001}})";
  WriteStateFile(kContent, /*file_mode=*/0620);

  EXPECT_FALSE(CreateTracker().Add({"3", "alice", 1000}).has_value());
  EXPECT_EQ(ReadStateFile(), kContent);
}

TEST_F(CreatedSessionTrackerTest, RemoveRemovesOnlyGivenSessionIds) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"1", "alice", 1000}).has_value());
  ASSERT_TRUE(tracker.Add({"2", "bob", 1001}).has_value());
  ASSERT_TRUE(tracker.Add({"3", "carol", 1002}).has_value());

  std::vector<std::string> to_remove = {"1", "3", "unknown"};
  ASSERT_TRUE(tracker.Remove(to_remove).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"2", "bob", 1001}));
}

TEST_F(CreatedSessionTrackerTest, RemoveUnknownSessionIdKeepsEntries) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"1", "alice", 1000}).has_value());

  std::vector<std::string> to_remove = {"2"};
  ASSERT_TRUE(tracker.Remove(to_remove).has_value());

  auto entries = tracker.Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"1", "alice", 1000}));
}

TEST_F(CreatedSessionTrackerTest, RemovingLastEntryDeletesFile) {
  auto tracker = CreateTracker();
  ASSERT_TRUE(tracker.Add({"1", "alice", 1000}).has_value());
  ASSERT_TRUE(base::PathExists(state_file_));

  std::vector<std::string> to_remove = {"1"};
  ASSERT_TRUE(tracker.Remove(to_remove).has_value());

  EXPECT_FALSE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, RemoveSucceedsIfDirectoryIsMissing) {
  ASSERT_TRUE(base::DeletePathRecursively(state_dir_));

  std::vector<std::string> to_remove = {"1"};
  EXPECT_TRUE(CreateTracker().Remove(to_remove).has_value());
  EXPECT_FALSE(base::PathExists(state_dir_));
}

TEST_F(CreatedSessionTrackerTest, RemoveDeletesMalformedFile) {
  WriteStateFile(R"({"1": "not a dict"})");

  std::vector<std::string> to_remove = {"2"};
  ASSERT_TRUE(CreateTracker().Remove(to_remove).has_value());

  EXPECT_FALSE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, RemoveFailsIfDirectoryIsSymlink) {
  base::FilePath real_dir = ReplaceStateDirectoryWithSymlink();
  base::FilePath real_file = real_dir.Append("created_sessions.json");
  ASSERT_TRUE(base::WriteFile(real_file,
                              R"({"1": {"username": "alice", "uid": 1000}})"));

  std::vector<std::string> to_remove = {"1"};
  EXPECT_FALSE(CreateTracker().Remove(to_remove).has_value());
  EXPECT_TRUE(base::PathExists(real_file));
}

TEST_F(CreatedSessionTrackerTest, RemoveFailsIfDirectoryHasWrongOwner) {
  WriteStateFile(R"({"1": {"username": "alice", "uid": 1000}})");

  CreatedSessionTracker tracker(state_file_, current_uid_ + 1);
  std::vector<std::string> to_remove = {"1"};
  EXPECT_FALSE(tracker.Remove(to_remove).has_value());
  EXPECT_TRUE(base::PathExists(state_file_));
}

TEST_F(CreatedSessionTrackerTest, LoadAcceptsValidFiles) {
  WriteStateFile(
      R"({"c1": {"username": "alice.b", "uid": 1000},
          "2": {"username": "bob", "uid": 4294967294.0}})");

  auto entries = CreateTracker().Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, ElementsAre(Entry{"2", "bob", 4294967294u},
                                    Entry{"c1", "alice.b", 1000}));

  WriteStateFile("{}");
  entries = CreateTracker().Load();
  ASSERT_TRUE(entries.has_value()) << entries.error();
  EXPECT_THAT(*entries, IsEmpty());
}

TEST_F(CreatedSessionTrackerTest, LoadRejectsMalformedFiles) {
  const char* const kMalformedFiles[] = {
      "not json",
      "[]",
      R"({"3": "alice"})",
      R"({"3": {"username": "alice"}})",
      R"({"3": {"uid": 1000}})",
      R"({"3": {"username": "alice", "uid": "1000"}})",
      R"({"3": {"username": "alice", "uid": 1000.5}})",
      R"({"3": {"username": "alice", "uid": -1}})",
      R"({"3": {"username": "alice", "uid": 4294967295}})",
      R"({"3": {"username": "alice", "uid": 4294967296}})",
      R"({"": {"username": "alice", "uid": 1000}})",
      R"({"3/4": {"username": "alice", "uid": 1000}})",
      R"({"3": {"username": "../alice", "uid": 1000}})",
  };
  for (const char* content : kMalformedFiles) {
    SCOPED_TRACE(content);
    WriteStateFile(content);
    EXPECT_FALSE(CreateTracker().Load().has_value());
  }
}

TEST_F(CreatedSessionTrackerTest, LoadRejectsInsecureFile) {
  // The security checks are covered by SecureFileReaderTest. This verifies
  // that Load() uses them.
  WriteStateFile(R"({"1": {"username": "alice", "uid": 1000}})",
                 /*file_mode=*/0620);
  EXPECT_FALSE(CreateTracker().Load().has_value());
}

}  // namespace remoting
