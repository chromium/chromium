// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>
#elif BUILDFLAG(IS_POSIX)
#include <sys/stat.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/format_macros.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher_inotify.h"
#endif

namespace content {

namespace {

base::AtomicSequenceNumber g_next_delegate_id;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN)
// inotify fires two events - one for each file creation + modification.
constexpr size_t kExpectedEventsForNewFileWrite = 2;
#else
constexpr size_t kExpectedEventsForNewFileWrite = 1;
#endif

enum class ExpectedEventsSinceLastWait { kNone, kSome };

struct Event {
  bool error;
  base::FilePath path;
  FilePathWatcher::ChangeInfo change_info;

  bool operator==(const Event& other) const {
    return error == other.error && path == other.path &&
           change_info == other.change_info;
  }
};
using EventListMatcher = testing::Matcher<std::list<Event>>;

Event ToEvent(const FilePathWatcher::ChangeInfo& change_info,
              const base::FilePath& path,
              bool error) {
  return Event{.error = error, .path = path, .change_info = change_info};
}

std::ostream& operator<<(std::ostream& os,
                         const FilePathWatcher::ChangeType& change_type) {
  switch (change_type) {
    case FilePathWatcher::ChangeType::kUnknown:
      return os << "unknown";
    case FilePathWatcher::ChangeType::kCreated:
      return os << "created";
    case FilePathWatcher::ChangeType::kDeleted:
      return os << "deleted";
    case FilePathWatcher::ChangeType::kModified:
      return os << "modified";
    case FilePathWatcher::ChangeType::kMoved:
      return os << "moved";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const FilePathWatcher::FilePathType& file_path_type) {
  switch (file_path_type) {
    case FilePathWatcher::FilePathType::kUnknown:
      return os << "Unknown";
    case FilePathWatcher::FilePathType::kFile:
      return os << "File";
    case FilePathWatcher::FilePathType::kDirectory:
      return os << "Directory";
  }
}

std::ostream& operator<<(std::ostream& os,
                         const FilePathWatcher::ChangeInfo& change_info) {
  return os << "ChangeInfo{ file_path_type: " << change_info.file_path_type
            << ", change_type: " << change_info.change_type
            << ", modified_path: " << change_info.modified_path
            << ", optional moved_from_path: "
            << change_info.moved_from_path.value_or(base::FilePath()) << " }";
}

std::ostream& operator<<(std::ostream& os, const Event& event) {
  if (event.error) {
    return os << "Event{ ERROR }";
  }

  return os << "Event{ path: " << event.path
            << ", change_info: " << event.change_info << " }";
}

void SpinEventLoopForABit() {
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), TestTimeouts::tiny_timeout());
  loop.Run();
}

// Returns the reason why `value` matches, or doesn't match, `matcher`.
template <typename MatcherType, typename Value>
std::string Explain(const MatcherType& matcher, const Value& value) {
  testing::StringMatchResultListener listener;
  testing::ExplainMatchResult(matcher, value, &listener);
  return listener.str();
}

inline constexpr auto HasPath = [](const base::FilePath& path) {
  return testing::Field(&Event::path, path);
};
inline constexpr auto HasErrored = []() {
  return testing::Field(&Event::error, testing::IsTrue());
};
inline constexpr auto HasModifiedPath = [](const base::FilePath& path) {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::modified_path, path));
};
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
inline constexpr auto HasMovedFromPath = [](const base::FilePath& path) {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::moved_from_path, path));
};
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
inline constexpr auto HasNoMovedFromPath = []() {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::moved_from_path,
                     testing::IsFalse()));
};
inline constexpr auto IsType =
    [](const FilePathWatcher::ChangeType& change_type) {
      return testing::Field(
          &Event::change_info,
          testing::Field(&FilePathWatcher::ChangeInfo::change_type,
                         change_type));
    };

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN)
inline constexpr auto IsFile = []() {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::file_path_type,
                     FilePathWatcher::FilePathType::kFile));
};
inline constexpr auto IsDirectory = []() {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::file_path_type,
                     FilePathWatcher::FilePathType::kDirectory));
};
#endif

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
inline constexpr auto IsUnknownPathType = []() {
  return testing::Field(
      &Event::change_info,
      testing::Field(&FilePathWatcher::ChangeInfo::file_path_type,
                     FilePathWatcher::FilePathType::kUnknown));
};
#endif

// When FSEvents reports an event as a result of the standalone
// `kFSEventStreamEventFlagRootChanged` event, there are no other flags (besides
// the root changed flag itself) to process. In this case, the file path type
// will evaluate to `kUnknown`.
#if BUILDFLAG(IS_MAC)
inline constexpr auto IsFile = []() {
  return testing::AnyOf(
      testing::Field(
          &Event::change_info,
          testing::Field(&FilePathWatcher::ChangeInfo::file_path_type,
                         FilePathWatcher::FilePathType::kFile)),
      IsUnknownPathType());
};
inline constexpr auto IsDirectory = []() {
  return testing::AnyOf(
      testing::Field(
          &Event::change_info,
          testing::Field(&FilePathWatcher::ChangeInfo::file_path_type,
                         FilePathWatcher::FilePathType::kDirectory)),
      IsUnknownPathType());
};
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
inline constexpr auto IsDeletedFile = IsFile;
inline constexpr auto IsDeletedDirectory = IsDirectory;

// TODO(crbug.com/341372596): A file move is reported as a directory on linux.
inline constexpr auto IsMovedFile = IsDirectory;
inline constexpr auto ModifiedMatcher = [](base::FilePath reported_path,
                                           base::FilePath modified_path) {
  return testing::ElementsAre(
      testing::AllOf(HasPath(reported_path), testing::Not(HasErrored()),
                     IsFile(), IsType(FilePathWatcher::ChangeType::kModified),
                     HasModifiedPath(modified_path), HasNoMovedFromPath()));
};

#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Windows figures out if a file path is a directory or file with `GetFileInfo`,
// but since the file is deleted, it can't know.
//
// This also needs to be used for events for a deleted file before it's actually
// deleted since the file path type still can't be determined.
inline constexpr auto IsDeletedFile = []() {
  return testing::AnyOf(IsFile(), IsUnknownPathType());
};
inline constexpr auto IsDeletedDirectory = []() {
  return testing::AnyOf(IsDirectory(), IsUnknownPathType());
};

#if BUILDFLAG(IS_MAC)
inline constexpr auto ModifiedMatcher = [](base::FilePath reported_path,
                                           base::FilePath modified_path) {
  return testing::ElementsAre(
      testing::AllOf(HasPath(reported_path), testing::Not(HasErrored()),
                     IsFile(), IsType(FilePathWatcher::ChangeType::kModified),
                     HasModifiedPath(modified_path), HasNoMovedFromPath()));
};
#else

inline constexpr auto IsMovedFile = IsFile;

// WriteFile causes two writes on Windows because it calls two syscalls:
// ::CreateFile and ::WriteFile.
inline constexpr auto ModifiedMatcher = [](base::FilePath reported_path,
                                           base::FilePath modified_path) {
  const auto modified_matcher =
      testing::AllOf(HasPath(reported_path), testing::Not(HasErrored()),
                     IsFile(), IsType(FilePathWatcher::ChangeType::kModified),
                     HasModifiedPath(modified_path), HasNoMovedFromPath());
  return testing::ElementsAreArray({modified_matcher, modified_matcher});
};
#endif
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

// Enables an accumulative, add-as-you-go pattern for expecting events:
//   - Do something that should fire `event1` on `delegate`
//   - Add `event1` to an `accumulated_event_expecter`
//   - Wait until `delegate` matches { `event1` }
//   - Do something that should fire `event2` on `delegate`
//   - Add `event2` to an `accumulated_event_expecter`
//   - Wait until `delegate` matches { `event1`, `event2` }
//   - ...
//
// These tests use an accumulative pattern due to the potential for
// false-positives, given that all we know is the number of changes at a given
// path (which is often fixed) and whether or not an error occurred (which is
// rare).
//
// TODO(crbug.com/40260973): This is not a common pattern. Generally,
// expectations are specified all-in-one at the start of a test, like so:
//   - Expect events { `event1`, `event2` }
//   - Do something that should fire `event1` on `delegate`
//   - Do something that should fire `event2` on `delegate`
//   - Wait until `delegate` matches { `event1`, `event2` }
//
// The potential for false-positives is much less if event types are known. We
// should consider moving towards the latter pattern
// (see `FilePathWatcherWithChangeInfoTest`) once that is supported.
class AccumulatingEventExpecter {
 public:
  EventListMatcher GetMatcher() {
    return testing::ContainerEq(expected_events_);
  }

  EventListMatcher GetFailureMatcher() {
    return testing::Not(testing::IsSubsetOf(expected_events_));
  }

  ExpectedEventsSinceLastWait GetAndResetExpectedEventsSinceLastWait() {
    auto temp = expected_events_since_last_wait_;
    expected_events_since_last_wait_ = ExpectedEventsSinceLastWait::kNone;
    return temp;
  }

  void AddExpectedEventForPath(const base::FilePath& path, bool error = false) {
    expected_events_.emplace_back(
        ToEvent(FilePathWatcher::ChangeInfo(), path, error));
    expected_events_since_last_wait_ = ExpectedEventsSinceLastWait::kSome;
  }

 private:
  std::list<Event> expected_events_;
  ExpectedEventsSinceLastWait expected_events_since_last_wait_ =
      ExpectedEventsSinceLastWait::kNone;
};

class TestDelegateBase {
 public:
  TestDelegateBase() = default;
  TestDelegateBase(const TestDelegateBase&) = delete;
  TestDelegateBase& operator=(const TestDelegateBase&) = delete;
  virtual ~TestDelegateBase() = default;

  virtual void OnFileChanged(const base::FilePath& path, bool error) = 0;
  virtual void OnFileChangedWithInfo(
      const FilePathWatcher::ChangeInfo& change_info,
      const base::FilePath& path,
      bool error) = 0;
  virtual base::WeakPtr<TestDelegateBase> AsWeakPtr() = 0;
};

// Receives and accumulates notifications from a specific `FilePathWatcher`.
// This class is not thread safe. All methods must be called from the sequence
// the instance is constructed on.
class TestDelegate final : public TestDelegateBase {
 public:
  TestDelegate() : id_(g_next_delegate_id.GetNext()) {}
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  // TestDelegateBase:
  void OnFileChanged(const base::FilePath& path, bool error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Event event = ToEvent(FilePathWatcher::ChangeInfo(), path, error);
    received_events_.emplace_back(std::move(event));
  }
  void OnFileChangedWithInfo(const FilePathWatcher::ChangeInfo& change_info,
                             const base::FilePath& path,
                             bool error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Event event = ToEvent(change_info, path, error);
    received_events_.emplace_back(std::move(event));
  }

  base::WeakPtr<TestDelegateBase> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Gives all in-flight events a chance to arrive, then forgets all events that
  // have been received by this delegate. This method may be a useful reset
  // after performing a file system operation that may result in a variable
  // sequence of events.
  void SpinAndDiscardAllReceivedEvents() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SpinEventLoopForABit();
    received_events_.clear();
  }

  // Spin the event loop until `received_events_` matches `matcher`,
  // `received_events_` matches `failure_matcher`, or we time out. The
  // `failure_matcher` matches when its not possible for `matcher` to match.
  void RunUntilEventsMatch(
      const EventListMatcher& matcher,
      const EventListMatcher& failure_matcher,
      ExpectedEventsSinceLastWait expected_events_since_last_wait,
      const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (expected_events_since_last_wait == ExpectedEventsSinceLastWait::kNone) {
      // Give unexpected events a chance to arrive.
      SpinEventLoopForABit();
    }

    EXPECT_TRUE(base::test::RunUntil([&]() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return testing::Matches(matcher)(received_events_) ||
             testing::Matches(failure_matcher)(received_events_);
    })) << "Timed out attempting to match events at "
        << location.file_name() << ":" << location.line_number() << std::endl
        << Explain(matcher, received_events_);
    EXPECT_TRUE(testing::Matches(matcher)(received_events_))
        << "Failed to match events at " << location.file_name() << ":"
        << location.line_number() << std::endl
        << Explain(matcher, received_events_);
  }

  // Convenience method for above.
  void RunUntilEventsMatch(
      const EventListMatcher& matcher,
      ExpectedEventsSinceLastWait expected_events_since_last_wait,
      const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    RunUntilEventsMatch(matcher, testing::SizeIs(-1),
                        expected_events_since_last_wait, location);
  }

  // Convenience method for above.
  void RunUntilEventsMatch(const EventListMatcher& matcher,
                           const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return RunUntilEventsMatch(matcher, ExpectedEventsSinceLastWait::kSome,
                               location);
  }

  // Convenience method for above.
  void RunUntilEventsMatch(AccumulatingEventExpecter& event_expecter,
                           const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return RunUntilEventsMatch(
        event_expecter.GetMatcher(), event_expecter.GetFailureMatcher(),
        event_expecter.GetAndResetExpectedEventsSinceLastWait(), location);
  }

  // Convenience method for above when no events are expected.
  void SpinAndExpectNoEvents(const base::Location& location = FROM_HERE) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return RunUntilEventsMatch(testing::IsEmpty(),
                               ExpectedEventsSinceLastWait::kNone, location);
  }

  const std::list<Event>& events() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return received_events_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Uniquely generated ID used to tie events to this delegate.
  const size_t id_;

  std::list<Event> received_events_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<TestDelegateBase> weak_ptr_factory_{this};
};

}  // namespace

#if BUILDFLAG(IS_FUCHSIA)
// FilePatchWatcherImpl is not implemented (see crbug.com/851641).
// Disable all tests.
#define FilePathWatcherTest DISABLED_FilePathWatcherTest
#endif

class FilePathWatcherTest : public testing::Test {
 public:
  FilePathWatcherTest()
#if BUILDFLAG(IS_POSIX)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO)
#endif
  {
  }

  FilePathWatcherTest(const FilePathWatcherTest&) = delete;
  FilePathWatcherTest& operator=(const FilePathWatcherTest&) = delete;
  ~FilePathWatcherTest() override = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    // Watching files is only permitted when all parent directories are
    // accessible, which is not the case for the default temp directory
    // on Android which is under /data/data.  Use /sdcard instead.
    // TODO(pauljensen): Remove this when crbug.com/475568 is fixed.
    base::FilePath parent_dir;
    ASSERT_TRUE(base::android::GetExternalStorageDirectory(&parent_dir));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(parent_dir));
#elif BUILDFLAG(IS_MAC)
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
#else
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#endif
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  base::FilePath test_file() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest");
  }

  base::FilePath test_link() {
    return temp_dir_.GetPath().AppendASCII("FilePathWatcherTest.lnk");
  }

  bool CreateDirectory(const base::FilePath& full_path) {
    bool result = base::CreateDirectory(full_path);
#if BUILDFLAG(IS_MAC)
    // Wait so that the event for this operation is received by FSEvents before
    // returning.
    SpinEventLoopForABit();
#endif
    return result;
  }

  bool WriteFile(const base::FilePath& filename, std::string_view data) {
    bool result = base::WriteFile(filename, data);
#if BUILDFLAG(IS_MAC)
    // Wait so that the event for this operation is received by FSEvents before
    // returning.
    SpinEventLoopForABit();
#endif
    return result;
  }

  bool DeleteFile(const base::FilePath& path) {
    bool result = base::DeleteFile(path);
#if BUILDFLAG(IS_MAC)
    // Wait so that the event for this operation is received by FSEvents before
    // returning.
    SpinEventLoopForABit();
#endif
    return result;
  }

  bool DeletePathRecursively(const base::FilePath& path) {
    bool result = base::DeletePathRecursively(path);
#if BUILDFLAG(IS_MAC)
    // Wait so that the event for this operation is received by FSEvents before
    // returning.
    SpinEventLoopForABit();
#endif
    return result;
  }

  bool Move(const base::FilePath& from_path, const base::FilePath& to_path) {
    bool result = base::Move(from_path, to_path);
#if BUILDFLAG(IS_MAC)
    // Wait so that the event for this operation is received by FSEvents before
    // returning.
    SpinEventLoopForABit();
#endif
    return result;
  }

  bool SetupWatch(const base::FilePath& target,
                  FilePathWatcher* watcher,
                  TestDelegateBase* delegate,
                  FilePathWatcher::Type watch_type);

  bool SetupWatchWithOptions(const base::FilePath& target,
                             FilePathWatcher* watcher,
                             TestDelegateBase* delegate,
                             FilePathWatcher::WatchOptions watch_options);

  bool SetupWatchWithChangeInfo(const base::FilePath& target,
                                FilePathWatcher* watcher,
                                TestDelegateBase* delegate,
                                FilePathWatcher::WatchOptions watch_options);

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
};

bool FilePathWatcherTest::SetupWatch(const base::FilePath& target,
                                     FilePathWatcher* watcher,
                                     TestDelegateBase* delegate,
                                     FilePathWatcher::Type watch_type) {
  return watcher->Watch(target, watch_type,
                        base::BindRepeating(&TestDelegateBase::OnFileChanged,
                                            delegate->AsWeakPtr()));
}

bool FilePathWatcherTest::SetupWatchWithOptions(
    const base::FilePath& target,
    FilePathWatcher* watcher,
    TestDelegateBase* delegate,
    FilePathWatcher::WatchOptions watch_options) {
#if BUILDFLAG(IS_MAC)
  // Flush events before the watch begins.
  SpinEventLoopForABit();
#endif
  return watcher->WatchWithOptions(
      target, watch_options,
      base::BindRepeating(&TestDelegateBase::OnFileChanged,
                          delegate->AsWeakPtr()));
}

bool FilePathWatcherTest::SetupWatchWithChangeInfo(
    const base::FilePath& target,
    FilePathWatcher* watcher,
    TestDelegateBase* delegate,
    FilePathWatcher::WatchOptions watch_options) {
#if BUILDFLAG(IS_MAC)
  // Flush events before the watch begins.
  SpinEventLoopForABit();
#endif
  return watcher->WatchWithChangeInfo(
      target, watch_options,
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &TestDelegateBase::OnFileChangedWithInfo, delegate->AsWeakPtr())));
}

// Basic test: Create the file and verify that we notice.
TEST_F(FilePathWatcherTest, NewFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_file());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// Basic test: Create the directory and verify that we notice.
TEST_F(FilePathWatcherTest, NewDirectory) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Basic test: Create the directory and verify that we notice.
TEST_F(FilePathWatcherTest, NewDirectoryRecursiveWatch) {
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    return;
  }

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kRecursive));

  ASSERT_TRUE(CreateDirectory(test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that modifying the file is caught.
TEST_F(FilePathWatcherTest, ModifiedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
#if BUILDFLAG(IS_WIN)
  // WriteFile causes two writes on Windows because it calls two syscalls:
  // ::CreateFile and ::WriteFile.
  event_expecter.AddExpectedEventForPath(test_file());
#endif
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that creating the parent directory of the watched file is not caught.
TEST_F(FilePathWatcherTest, CreateParentDirectory) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));

  ASSERT_TRUE(SetupWatch(child, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we do not get notified when the parent is created.
  ASSERT_TRUE(CreateDirectory(parent));
  delegate.SpinAndExpectNoEvents();
}

// Verify that changes to the sibling of the watched file are not caught.
TEST_F(FilePathWatcherTest, CreateSiblingFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we do not get notified if a sibling of the watched file is
  // created or modified.
  ASSERT_TRUE(WriteFile(test_file().AddExtensionASCII(".swap"), "content"));
  ASSERT_TRUE(WriteFile(test_file().AddExtensionASCII(".swap"), "new content"));
  delegate.SpinAndExpectNoEvents();
}

// Verify that changes to the sibling of the parent of the watched file are not
// caught.
TEST_F(FilePathWatcherTest, CreateParentSiblingFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath parent_sibling(
      temp_dir_.GetPath().AppendASCII("parent_sibling"));
  base::FilePath child(parent.AppendASCII("child"));
  ASSERT_TRUE(SetupWatch(child, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Don't notice changes to a sibling directory of `parent` while `parent` does
  // not exist.
  ASSERT_TRUE(CreateDirectory(parent_sibling));
  ASSERT_TRUE(DeletePathRecursively(parent_sibling));

  // Don't notice changes to a sibling file of `parent` while `parent` does
  // not exist.
  ASSERT_TRUE(WriteFile(parent_sibling, "do not notice this"));
  ASSERT_TRUE(DeleteFile(parent_sibling));

  // Don't notice the creation of `parent`.
  ASSERT_TRUE(CreateDirectory(parent));

  // Don't notice changes to a sibling directory of `parent` while `parent`
  // exists.
  ASSERT_TRUE(CreateDirectory(parent_sibling));
  ASSERT_TRUE(DeletePathRecursively(parent_sibling));

  // Don't notice changes to a sibling file of `parent` while `parent` exists.
  ASSERT_TRUE(WriteFile(parent_sibling, "do not notice this"));
  ASSERT_TRUE(DeleteFile(parent_sibling));

  delegate.SpinAndExpectNoEvents();
}

// Verify that moving an unwatched file to a watched path is caught.
TEST_F(FilePathWatcherTest, MovedToFile) {
  base::FilePath source_file(temp_dir_.GetPath().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is moved.
  ASSERT_TRUE(Move(source_file, test_file()));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that moving the watched file to an unwatched path is caught.
TEST_F(FilePathWatcherTest, MovedFromFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(Move(test_file(), temp_dir_.GetPath().AppendASCII("dest")));
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is deleted.
  DeleteFile(test_file());
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}

#if BUILDFLAG(IS_WIN)
TEST_F(FilePathWatcherTest, WindowsBufferOverflow) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  {
    // Block the Watch thread.
    base::AutoLock auto_lock(watcher.GetWatchThreadLockForTest());

    // Generate an event that will try to acquire the lock on the watch thread.
    ASSERT_TRUE(WriteFile(test_file(), "content"));

    // The packet size plus the path size. `WriteFile` generates two events so
    // it's twice that.
    const size_t kWriteFileEventSize =
        (sizeof(FILE_NOTIFY_INFORMATION) + test_file().AsUTF8Unsafe().size()) *
        2;

    // The max size that's allowed for network drives:
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw#remarks.
    const size_t kMaxBufferSize = 64 * 1024;

    for (size_t bytes_in_buffer = 0; bytes_in_buffer < kMaxBufferSize;
         bytes_in_buffer += kWriteFileEventSize) {
      WriteFile(test_file(), "content");
    }
  }

  // The initial `WriteFile` generates an event.
  event_expecter.AddExpectedEventForPath(test_file());
  // The rest should only appear as a buffer overflow.
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);
}
#endif

namespace {

// Used by the DeleteDuringNotify test below.
// Deletes the FilePathWatcher when it's notified.
class Deleter final : public TestDelegateBase {
 public:
  explicit Deleter(base::OnceClosure done_closure)
      : watcher_(std::make_unique<FilePathWatcher>()),
        done_closure_(std::move(done_closure)) {}
  Deleter(const Deleter&) = delete;
  Deleter& operator=(const Deleter&) = delete;
  ~Deleter() override = default;

  void OnFileChanged(const base::FilePath& /*path*/, bool /*error*/) override {
    watcher_.reset();
    std::move(done_closure_).Run();
  }
  void OnFileChangedWithInfo(const FilePathWatcher::ChangeInfo& /*change_info*/,
                             const base::FilePath& /*path*/,
                             bool /*error*/) override {
    watcher_.reset();
    std::move(done_closure_).Run();
  }

  base::WeakPtr<TestDelegateBase> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  FilePathWatcher* watcher() const { return watcher_.get(); }

 private:
  std::unique_ptr<FilePathWatcher> watcher_;
  base::OnceClosure done_closure_;
  base::WeakPtrFactory<Deleter> weak_ptr_factory_{this};
};

}  // namespace

// Verify that deleting a watcher during the callback doesn't crash.
TEST_F(FilePathWatcherTest, DeleteDuringNotify) {
  base::RunLoop run_loop;
  Deleter deleter(run_loop.QuitClosure());
  ASSERT_TRUE(SetupWatch(test_file(), deleter.watcher(), &deleter,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  run_loop.Run();

  // We win if we haven't crashed yet.
  // Might as well double-check it got deleted, too.
  ASSERT_TRUE(deleter.watcher() == nullptr);
}

// Verify that deleting the watcher works even if there is a pending
// notification.
TEST_F(FilePathWatcherTest, DestroyWithPendingNotification) {
  TestDelegate delegate;
  FilePathWatcher watcher;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(WriteFile(test_file(), "content"));
}

TEST_F(FilePathWatcherTest, MultipleWatchersSingleFile) {
  FilePathWatcher watcher1, watcher2;
  TestDelegate delegate1, delegate2;
  AccumulatingEventExpecter event_expecter1, event_expecter2;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher1, &delegate1,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(test_file(), &watcher2, &delegate2,
                         FilePathWatcher::Type::kNonRecursive));

  // Expect to be notified for writing to a new file for each delegate.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter1.AddExpectedEventForPath(test_file());
    event_expecter2.AddExpectedEventForPath(test_file());
  }
  delegate1.RunUntilEventsMatch(event_expecter1);
  delegate2.RunUntilEventsMatch(event_expecter2);
}

// Verify that watching a file whose parent directory doesn't exist yet works if
// the directory and file are created eventually.
TEST_F(FilePathWatcherTest, NonExistentDirectory) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file(dir.AppendASCII("file"));
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // The delegate is only watching the file. Parent directory creation should
  // not trigger an event.
  ASSERT_TRUE(CreateDirectory(dir));
  // TODO(crbug.com/40263777): Expect that no events are fired.

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
#if BUILDFLAG(IS_WIN)
  // WriteFile causes two writes on Windows because it calls two syscalls:
  // ::CreateFile and ::WriteFile.
  event_expecter.AddExpectedEventForPath(file);
#endif
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Exercises watch reconfiguration for the case that directories on the path
// are rapidly created.
TEST_F(FilePathWatcherTest, DirectoryChain) {
  base::FilePath path(temp_dir_.GetPath());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(base::StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }

  FilePathWatcher watcher;
  base::FilePath file(path.AppendASCII("file"));
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  base::FilePath sub_path(temp_dir_.GetPath());
  for (const auto& dir_name : dir_names) {
    sub_path = sub_path.AppendASCII(dir_name);
    ASSERT_TRUE(CreateDirectory(sub_path));
    // TODO(crbug.com/40263777): Expect that no events are fired.
  }

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  VLOG(1) << "Create File";
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation + modification";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file modification";
#if BUILDFLAG(IS_WIN)
  // WriteFile causes two writes on Windows because it calls two syscalls:
  // ::CreateFile and ::WriteFile.
  event_expecter.AddExpectedEventForPath(file);
#endif
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Windows doesn't allow the target directory to be deleted while there is a
// FilePathWatcher watching it.
#if !BUILDFLAG(IS_WIN)
TEST_F(FilePathWatcherTest, DisappearingDirectory) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file(dir.AppendASCII("file"));
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(DeletePathRecursively(dir));
  event_expecter.AddExpectedEventForPath(file);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40263766): Figure out why this may fire two events on
  // inotify. Only the file is being watched, so presumably there should only be
  // one deletion event.
  event_expecter.AddExpectedEventForPath(file);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
  delegate.RunUntilEventsMatch(event_expecter);
}
#endif

// Tests that a file that is deleted and reappears is tracked correctly.
TEST_F(FilePathWatcherTest, DeleteAndRecreate) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(DeleteFile(test_file()));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(test_file());
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  VLOG(1) << "Waiting for file creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_file());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// TODO(crbug.com/40263777): Split into smaller tests.
TEST_F(FilePathWatcherTest, WatchDirectory) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file1(dir.AppendASCII("file1"));
  base::FilePath file2(dir.AppendASCII("file2"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(dir, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(dir));
  VLOG(1) << "Waiting for directory creation";
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file1, "content"));
  VLOG(1) << "Waiting for file1 creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

#if !BUILDFLAG(IS_APPLE)
  ASSERT_TRUE(WriteFile(file1, "content v2"));
  // Mac implementation does not detect files modified in a directory.
  // TODO(crbug.com/40263777): Expect that no events are fired on Mac.
  // TODO(crbug.com/40105284): Consider using FSEvents to support
  // watching a directory and its immediate children, as Type::kNonRecursive
  // does on other platforms.
  VLOG(1) << "Waiting for file1 modification";
  event_expecter.AddExpectedEventForPath(dir);
#if BUILDFLAG(IS_WIN)
  // WriteFile causes two writes on Windows because it calls two syscalls:
  // ::CreateFile and ::WriteFile.
  event_expecter.AddExpectedEventForPath(dir);
#endif
  delegate.RunUntilEventsMatch(event_expecter);
#endif  // !BUILDFLAG(IS_APPLE)

  ASSERT_TRUE(DeleteFile(file1));
  VLOG(1) << "Waiting for file1 deletion";
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file2, "content"));
  VLOG(1) << "Waiting for file2 creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, MoveParent) {
  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  AccumulatingEventExpecter file_event_expecter, subdir_event_expecter;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath dest(temp_dir_.GetPath().AppendASCII("dest"));
  base::FilePath subdir(dir.AppendASCII("subdir"));
  base::FilePath file(subdir.AppendASCII("file"));
  ASSERT_TRUE(SetupWatch(file, &file_watcher, &file_delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(subdir, &subdir_watcher, &subdir_delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Setup a directory hierarchy.
  // We should only get notified on `subdir_delegate` of its creation.
  ASSERT_TRUE(CreateDirectory(subdir));
  subdir_event_expecter.AddExpectedEventForPath(subdir);
  // TODO(crbug.com/40263777): Expect that no events are fired on the
  // file delegate.
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation + modification";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    file_event_expecter.AddExpectedEventForPath(file);
    subdir_event_expecter.AddExpectedEventForPath(subdir);
  }
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);

  Move(dir, dest);
  VLOG(1) << "Waiting for directory move";
  file_event_expecter.AddExpectedEventForPath(file);
  subdir_event_expecter.AddExpectedEventForPath(subdir);
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);
}

TEST_F(FilePathWatcherTest, RecursiveWatch) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  bool setup_result =
      SetupWatch(dir, &watcher, &delegate, FilePathWatcher::Type::kRecursive);
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    ASSERT_FALSE(setup_result);
    return;
  }
  ASSERT_TRUE(setup_result);

  // TODO(crbug.com/40263777): Create a version of this test which also
  // verifies that the events occur on the correct file path if the watcher is
  // set up to record the target of the event.

  // Main directory("dir") creation.
  ASSERT_TRUE(CreateDirectory(dir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/file1".
  base::FilePath file1(dir.AppendASCII("file1"));
  ASSERT_TRUE(WriteFile(file1, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir".
  base::FilePath subdir(dir.AppendASCII("subdir"));
  ASSERT_TRUE(CreateDirectory(subdir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir2".
  base::FilePath subdir2(subdir.AppendASCII("subdir2"));
  ASSERT_TRUE(CreateDirectory(subdir2));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Rename "$dir/subdir/subdir2" to "$dir/subdir/subdir2b".
  base::FilePath subdir2b(subdir.AppendASCII("subdir2b"));
  Move(subdir2, subdir2b);
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir_file1".
  base::FilePath subdir_file1(subdir.AppendASCII("subdir_file1"));
  ASSERT_TRUE(WriteFile(subdir_file1, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir_child_dir".
  base::FilePath subdir_child_dir(subdir.AppendASCII("subdir_child_dir"));
  ASSERT_TRUE(CreateDirectory(subdir_child_dir));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create "$dir/subdir/subdir_child_dir/child_dir_file1".
  base::FilePath child_dir_file1(
      subdir_child_dir.AppendASCII("child_dir_file1"));
  ASSERT_TRUE(WriteFile(child_dir_file1, "content v2"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(dir);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Write into "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(WriteFile(child_dir_file1, "content"));
  event_expecter.AddExpectedEventForPath(dir);
#if BUILDFLAG(IS_WIN)
  // WriteFile causes two writes on Windows because it calls two syscalls:
  // ::CreateFile and ::WriteFile.
  event_expecter.AddExpectedEventForPath(dir);
#endif
  delegate.RunUntilEventsMatch(event_expecter);

  // Delete "$dir/subdir/subdir_file1".
  ASSERT_TRUE(DeleteFile(subdir_file1));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);

  // Delete "$dir/subdir/subdir_child_dir/child_dir_file1".
  ASSERT_TRUE(DeleteFile(child_dir_file1));
  event_expecter.AddExpectedEventForPath(dir);
  delegate.RunUntilEventsMatch(event_expecter);
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
// Apps cannot create symlinks on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
//
// This test is disabled on Fuchsia since it doesn't support symlinking.
//
// This test is disabled on Mac since recursive watches aren't supported for
// symlinks.
TEST_F(FilePathWatcherTest, RecursiveWithSymLink) {
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    return;
  }

  FilePathWatcher watcher;
  base::FilePath test_dir(temp_dir_.GetPath().AppendASCII("test_dir"));
  ASSERT_TRUE(CreateDirectory(test_dir));
  base::FilePath symlink(test_dir.AppendASCII("symlink"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(symlink, &watcher, &delegate,
                         FilePathWatcher::Type::kRecursive));

  // TODO(crbug.com/40263777): Figure out what the intended behavior here
  // is. Many symlink operations don't seem to be supported on Mac, while in
  // other cases Mac fires more events than expected.

  // Link creation.
  base::FilePath target1(temp_dir_.GetPath().AppendASCII("target1"));
  ASSERT_TRUE(CreateSymbolicLink(target1, symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Target1 creation.
  ASSERT_TRUE(CreateDirectory(target1));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create a file in target1.
  base::FilePath target1_file(target1.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target1_file, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(symlink);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Link change.
  base::FilePath target2(temp_dir_.GetPath().AppendASCII("target2"));
  ASSERT_TRUE(CreateDirectory(target2));
  // TODO(crbug.com/40263777): Expect that no events are fired.

  ASSERT_TRUE(DeleteFile(symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(CreateSymbolicLink(target2, symlink));
  event_expecter.AddExpectedEventForPath(symlink);
  delegate.RunUntilEventsMatch(event_expecter);

  // Create a file in target2.
  base::FilePath target2_file(target2.AppendASCII("file"));
  ASSERT_TRUE(WriteFile(target2_file, "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(symlink);
  }
  delegate.RunUntilEventsMatch(event_expecter);
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)

TEST_F(FilePathWatcherTest, MoveChild) {
  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  AccumulatingEventExpecter file_event_expecter, subdir_event_expecter;
  base::FilePath source_dir(temp_dir_.GetPath().AppendASCII("source"));
  base::FilePath source_subdir(source_dir.AppendASCII("subdir"));
  base::FilePath source_file(source_subdir.AppendASCII("file"));
  base::FilePath dest_dir(temp_dir_.GetPath().AppendASCII("dest"));
  base::FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  base::FilePath dest_file(dest_subdir.AppendASCII("file"));

  // Setup a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  ASSERT_TRUE(SetupWatch(dest_file, &file_watcher, &file_delegate,
                         FilePathWatcher::Type::kNonRecursive));
  ASSERT_TRUE(SetupWatch(dest_subdir, &subdir_watcher, &subdir_delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(Move(source_dir, dest_dir));
  file_event_expecter.AddExpectedEventForPath(dest_file);
  subdir_event_expecter.AddExpectedEventForPath(dest_subdir);
  file_delegate.RunUntilEventsMatch(file_event_expecter);
  subdir_delegate.RunUntilEventsMatch(subdir_event_expecter);
}

TEST_F(FilePathWatcherTest, MoveOverwritingFile) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  base::FilePath to_path(temp_dir_.GetPath().AppendASCII("to"));
  base::FilePath from_path(temp_dir_.GetPath().AppendASCII("from"));

  // Setup a directory hierarchy.
  ASSERT_TRUE(WriteFile(to_path, "content1"));
  ASSERT_TRUE(WriteFile(from_path, "content2"));

  ASSERT_TRUE(SetupWatch(temp_dir_.GetPath(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Move the directory into place, s.t. the watched file appears.
  Move(from_path, to_path);

  // The move event.
  event_expecter.AddExpectedEventForPath(temp_dir_.GetPath());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that changing attributes on a file is caught
#if BUILDFLAG(IS_ANDROID)
// Apps cannot change file attributes on Android in /sdcard as /sdcard uses the
// "fuse" file system, while /data uses "ext4".  Running these tests in /data
// would be preferable and allow testing file attributes and symlinks.
// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#define FileAttributesChanged DISABLED_NoEventWhenFileAttributesChanged
#endif  // BUILDFLAG(IS_ANDROID)

// This test is disabled on Mac because we don't support reporting file metadata
// changes on FSEvents.
#if !BUILDFLAG(IS_MAC)
TEST_F(FilePathWatcherTest, NoEventWhenFileAttributesChanged) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(test_file(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(MakeFileUnreadable(test_file()));
  delegate.SpinAndExpectNoEvents();
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Verify that creating a symlink is caught.
TEST_F(FilePathWatcherTest, CreateLink) {
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is created.
  // Note that test_file() doesn't have to exist.
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that deleting a symlink is caught.
TEST_F(FilePathWatcherTest, DeleteLink) {
  // Unfortunately this test case only works if the link target exists.
  // TODO(craig) fix this as part of crbug.com/91561.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the link is deleted.
  ASSERT_TRUE(DeleteFile(test_link()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that modifying a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, ModifiedLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that creating a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, CreateTargetLinkedFile) {
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is created.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(test_link());
  }
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that deleting a target file that a link is pointing to
// when we are watching the link is caught.
TEST_F(FilePathWatcherTest, DeleteTargetLinkedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // Note that we are watching the symlink.
  ASSERT_TRUE(SetupWatch(test_link(), &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // Now make sure we get notified if the target file is deleted.
  ASSERT_TRUE(DeleteFile(test_file()));
  event_expecter.AddExpectedEventForPath(test_link());
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file whose parent directory is a link that
// doesn't exist yet works if the symlink is created eventually.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart1) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  // dir/file should exist.
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
  // Note that we are watching dir.lnk/file which doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  VLOG(1) << "Waiting for link creation";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file creation + modification";
  // TODO(crbug.com/40263777): Should this fire two events on inotify?
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file whose parent directory is a
// dangling symlink works if the directory is created eventually.
// TODO(crbug.com/40263777): Add test coverage for symlinked file
// creation independent of a corresponding write.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart2) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;

  // Now create the link from dir.lnk pointing to dir but
  // neither dir nor dir/file exist yet.
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(CreateDirectory(dir));
  // TODO(crbug.com/40263777): Expect that no events are fired.

  // It may take some time for `watcher` to re-construct its watch list, so it's
  // possible an event is missed. _At least_ one event should be fired, though.
  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  delegate.RunUntilEventsMatch(testing::Not(testing::IsEmpty()),
                               ExpectedEventsSinceLastWait::kSome);

  delegate.SpinAndDiscardAllReceivedEvents();
  AccumulatingEventExpecter event_expecter;

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Verify that watching a file with a symlink on the path
// to the file works.
TEST_F(FilePathWatcherTest, LinkedDirectoryPart3) {
  FilePathWatcher watcher;
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Note that we are watching dir.lnk/file but the file doesn't exist yet.
  ASSERT_TRUE(SetupWatch(linkfile, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  ASSERT_TRUE(WriteFile(file, "content"));
  VLOG(1) << "Waiting for file creation";
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(linkfile);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(WriteFile(file, "content v2"));
  VLOG(1) << "Waiting for file change";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(DeleteFile(file));
  VLOG(1) << "Waiting for file deletion";
  event_expecter.AddExpectedEventForPath(linkfile);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Regression tests that FilePathWatcherImpl does not leave its reference in
// `g_inotify_reader` due to a race in recursive watch.
// See https://crbug.com/990004.
TEST_F(FilePathWatcherTest, RacyRecursiveWatch) {
  if (!FilePathWatcher::RecursiveWatchAvailable()) {
    GTEST_SKIP();
  }

  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));

  // Create and delete many subdirs. 20 is an arbitrary number big enough
  // to have more chances to make FilePathWatcherImpl leak watchers.
  std::vector<base::FilePath> subdirs;
  for (int i = 0; i < 20; ++i) {
    subdirs.emplace_back(dir.AppendASCII(base::StringPrintf("subdir_%d", i)));
  }

  base::Thread subdir_updater("SubDir Updater");
  ASSERT_TRUE(subdir_updater.Start());

  auto subdir_update_task = base::BindLambdaForTesting([&]() {
    for (const auto& subdir : subdirs) {
      // First update event to trigger watch callback.
      ASSERT_TRUE(CreateDirectory(subdir));

      // Second update event. The notification sent for this event will race
      // with the upcoming deletion of the directory below. This test is about
      // verifying that the impl handles this.
      base::FilePath subdir_file(subdir.AppendASCII("subdir_file"));
      ASSERT_TRUE(WriteFile(subdir_file, "content"));

      // Racy subdir delete to trigger watcher leak.
      ASSERT_TRUE(DeletePathRecursively(subdir));
    }
  });

  // Try the racy subdir update 100 times.
  for (int i = 0; i < 100; ++i) {
    base::RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    // Keep watch callback in `watcher_callback` so that "watcher.reset()"
    // inside does not release the callback and the lambda capture with it.
    // Otherwise, accessing `run_loop` as part of the lambda capture would be
    // use-after-free under asan.
    auto watcher_callback =
        base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
          // Release watchers in callback so that the leaked watchers of
          // the subdir stays. Otherwise, when the subdir is deleted,
          // its delete event would clean up leaked watchers in
          // `g_inotify_reader`.
          watcher.reset();

          run_loop.Quit();
        });

    bool setup_result = watcher->Watch(dir, FilePathWatcher::Type::kRecursive,
                                       watcher_callback);
    ASSERT_TRUE(setup_result);

    subdir_updater.task_runner()->PostTask(FROM_HERE, subdir_update_task);

    // Wait for the watch callback.
    run_loop.Run();

    // `watcher` should have been released.
    ASSERT_FALSE(watcher);

    // There should be no outstanding watchers.
    ASSERT_FALSE(FilePathWatcher::HasWatchesForTest());
  }
}

// Verify that "Watch()" returns false and callback is not invoked when limit is
// hit during setup.
TEST_F(FilePathWatcherTest, InotifyLimitInWatch) {
  auto watcher = std::make_unique<FilePathWatcher>();

  // "test_file()" is like "/tmp/__unique_path__/FilePathWatcherTest" and has 4
  // dir components ("/" + 3 named parts). "Watch()" creates inotify watches
  // for each dir component of the given dir. It would fail with limit set to 1.
  ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(1);
  ASSERT_FALSE(watcher->Watch(
      test_file(), FilePathWatcher::Type::kNonRecursive,
      base::BindLambdaForTesting(
          [&](const base::FilePath& path, bool error) { ADD_FAILURE(); })));

  // Triggers update but callback should not be invoked.
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  // Ensures that the callback did not happen.
  base::RunLoop().RunUntilIdle();
}

// Verify that "error=true" callback happens when limit is hit during update.
TEST_F(FilePathWatcherTest, InotifyLimitInUpdate) {
  enum kTestType {
    // Destroy watcher in "error=true" callback.
    // No crash/deadlock when releasing watcher in the callback.
    kDestroyWatcher,

    // Do not destroy watcher in "error=true" callback.
    kDoNothing,
  };

  for (auto callback_type : {kDestroyWatcher, kDoNothing}) {
    SCOPED_TRACE(testing::Message() << "type=" << callback_type);

    base::RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error) {
            return;
          }

          error_callback_called = true;

          if (callback_type == kDestroyWatcher) {
            watcher.reset();
          }

          run_loop.Quit();
        });
    ASSERT_TRUE(watcher->Watch(
        test_file(), FilePathWatcher::Type::kNonRecursive, watcher_callback));

    ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(1);

    // Triggers update and over limit.
    ASSERT_TRUE(WriteFile(test_file(), "content"));

    run_loop.Run();

    // More update but no more callback should happen.
    ASSERT_TRUE(DeleteFile(test_file()));
    base::RunLoop().RunUntilIdle();
  }
}

// Similar to InotifyLimitInUpdate but test a recursive watcher.
TEST_F(FilePathWatcherTest, InotifyLimitInUpdateRecursive) {
  enum kTestType {
    // Destroy watcher in "error=true" callback.
    // No crash/deadlock when releasing watcher in the callback.
    kDestroyWatcher,

    // Do not destroy watcher in "error=true" callback.
    kDoNothing,
  };

  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));

  for (auto callback_type : {kDestroyWatcher, kDoNothing}) {
    SCOPED_TRACE(testing::Message() << "type=" << callback_type);

    base::RunLoop run_loop;
    auto watcher = std::make_unique<FilePathWatcher>();

    bool error_callback_called = false;
    auto watcher_callback =
        base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
          // No callback should happen after "error=true" one.
          ASSERT_FALSE(error_callback_called);

          if (!error) {
            return;
          }

          error_callback_called = true;

          if (callback_type == kDestroyWatcher) {
            watcher.reset();
          }

          run_loop.Quit();
        });
    ASSERT_TRUE(watcher->Watch(dir, FilePathWatcher::Type::kRecursive,
                               watcher_callback));

    constexpr size_t kMaxLimit = 10u;
    ScopedMaxNumberOfInotifyWatchesOverrideForTest max_inotify_watches(
        kMaxLimit);

    // Triggers updates and over limit.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      base::FilePath subdir =
          dir.AppendASCII(base::StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(CreateDirectory(subdir));
    }

    run_loop.Run();

    // More update but no more callback should happen.
    for (size_t i = 0; i < kMaxLimit; ++i) {
      base::FilePath subdir =
          dir.AppendASCII(base::StringPrintf("subdir_%" PRIuS, i));
      ASSERT_TRUE(DeleteFile(subdir));
    }
    base::RunLoop().RunUntilIdle();
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// TODO(fxbug.dev/60109): enable BUILDFLAG(IS_FUCHSIA) when implemented.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

TEST_F(FilePathWatcherTest, ReturnFullPath_RecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath watched_folder(
      temp_dir_.GetPath().AppendASCII("watched_folder"));
  base::FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatchWithOptions(watched_folder, &directory_watcher,
                                    &delegate,
                                    {.type = FilePathWatcher::Type::kRecursive,
                                     .report_modified_path = true}));

  // Triggers two events:
  // create on /watched_folder/file.
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_RecursiveInNestedFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath watched_folder(
      temp_dir_.GetPath().AppendASCII("watched_folder"));
  base::FilePath subfolder(watched_folder.AppendASCII("subfolder"));
  base::FilePath file(subfolder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatchWithOptions(watched_folder, &directory_watcher,
                                    &delegate,
                                    {.type = FilePathWatcher::Type::kRecursive,
                                     .report_modified_path = true}));

  // Expects create on /watched_folder/subfolder.
  ASSERT_TRUE(CreateDirectory(subfolder));
  event_expecter.AddExpectedEventForPath(subfolder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Triggers two events:
  // create on /watched_folder/subfolder/file.
  // modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/subfolder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/subfolder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/subfolder.
  ASSERT_TRUE(DeleteFile(subfolder));
  event_expecter.AddExpectedEventForPath(subfolder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath watched_folder(
      temp_dir_.GetPath().AppendASCII("watched_folder"));
  base::FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers two events:
  // create on /watched_folder/file.
  // modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(file);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder/file.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder/file.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(file);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnFullPath_NonRecursiveRemoveEnclosingFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath root_folder(temp_dir_.GetPath().AppendASCII("root_folder"));
  base::FilePath folder(root_folder.AppendASCII("folder"));
  base::FilePath watched_folder(folder.AppendASCII("watched_folder"));
  base::FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));
  ASSERT_TRUE(WriteFile(file, "test"));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive,
                             .report_modified_path = true}));

  // Triggers three events:
  // delete on /watched_folder/file.
  // delete on /watched_folder twice.
  // TODO(crbug.com/40263766): Figure out why duplicate events are fired
  // on `watched_folder`.
  ASSERT_TRUE(DeletePathRecursively(folder));
  event_expecter.AddExpectedEventForPath(file);
  event_expecter.AddExpectedEventForPath(watched_folder);
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_RecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath watched_folder(
      temp_dir_.GetPath().AppendASCII("watched_folder"));
  base::FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kRecursive}));

  // Triggers two events:
  // create on /watched_folder.
  // modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(watched_folder);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

TEST_F(FilePathWatcherTest, ReturnWatchedPath_NonRecursiveInRootFolder) {
  FilePathWatcher directory_watcher;
  base::FilePath watched_folder(
      temp_dir_.GetPath().AppendASCII("watched_folder"));
  base::FilePath file(watched_folder.AppendASCII("file"));

  ASSERT_TRUE(CreateDirectory(watched_folder));

  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(
      SetupWatchWithOptions(watched_folder, &directory_watcher, &delegate,
                            {.type = FilePathWatcher::Type::kNonRecursive}));

  // Triggers two events:
  // Expects create /watched_folder.
  // Expects modify /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test"));
  for (size_t i = 0; i < kExpectedEventsForNewFileWrite; ++i) {
    event_expecter.AddExpectedEventForPath(watched_folder);
  }
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects modify on /watched_folder.
  ASSERT_TRUE(WriteFile(file, "test123"));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);

  // Expects delete on /watched_folder.
  ASSERT_TRUE(DeleteFile(file));
  event_expecter.AddExpectedEventForPath(watched_folder);
  delegate.RunUntilEventsMatch(event_expecter);
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

namespace {

enum Permission { Read, Write, Execute };

#if BUILDFLAG(IS_APPLE)
bool ChangeFilePermissions(const base::FilePath& path,
                           Permission perm,
                           bool allow) {
  struct stat stat_buf;

  if (stat(path.value().c_str(), &stat_buf) != 0) {
    return false;
  }

  mode_t mode = 0;
  switch (perm) {
    case Read:
      mode = S_IRUSR | S_IRGRP | S_IROTH;
      break;
    case Write:
      mode = S_IWUSR | S_IWGRP | S_IWOTH;
      break;
    case Execute:
      mode = S_IXUSR | S_IXGRP | S_IXOTH;
      break;
    default:
      ADD_FAILURE() << "unknown perm " << perm;
      return false;
  }
  if (allow) {
    stat_buf.st_mode |= mode;
  } else {
    stat_buf.st_mode &= ~mode;
  }
  return chmod(path.value().c_str(), stat_buf.st_mode) == 0;
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace

#if BUILDFLAG(IS_APPLE)
// Linux implementation of FilePathWatcher doesn't catch attribute changes.
// http://crbug.com/78043
// Windows implementation of FilePathWatcher catches attribute changes that
// don't affect the path being watched.
// http://crbug.com/78045

// Verify that changing attributes on a directory works.
TEST_F(FilePathWatcherTest, DirAttributesChanged) {
  base::FilePath test_dir1(
      temp_dir_.GetPath().AppendASCII("DirAttributesChangedDir1"));
  base::FilePath test_dir2(test_dir1.AppendASCII("DirAttributesChangedDir2"));
  base::FilePath test_file(test_dir2.AppendASCII("DirAttributesChangedFile"));
  // Setup a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(test_dir1));
  ASSERT_TRUE(CreateDirectory(test_dir2));
  ASSERT_TRUE(WriteFile(test_file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(test_file, &watcher, &delegate,
                         FilePathWatcher::Type::kNonRecursive));

  // We should not get notified in this case as it hasn't affected our ability
  // to access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, false));
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Read, true));
  // TODO(crbug.com/40263777): Expect that no events are fired.

  // We should get notified in this case because filepathwatcher can no
  // longer access the file.
  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, false));
  event_expecter.AddExpectedEventForPath(test_file);
  delegate.RunUntilEventsMatch(event_expecter);

  ASSERT_TRUE(ChangeFilePermissions(test_dir1, Execute, true));
  // TODO(crbug.com/40263777): Expect that no events are fired.
}

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)

// Fail fast if trying to trivially watch a non-existent item.
TEST_F(FilePathWatcherTest, TrivialNoDir) {
  const base::FilePath tmp_dir = temp_dir_.GetPath();
  const base::FilePath non_existent = tmp_dir.Append(FILE_PATH_LITERAL("nope"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_FALSE(SetupWatch(non_existent, &watcher, &delegate,
                          FilePathWatcher::Type::kTrivial));
}

// Succeed starting a watch on a directory.
TEST_F(FilePathWatcherTest, TrivialDirStart) {
  const base::FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));
}

// Observe a change on a directory
TEST_F(FilePathWatcherTest, TrivialDirChange) {
  const base::FilePath tmp_dir = temp_dir_.GetPath();

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(tmp_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(TouchFile(tmp_dir, base::Time::Now(), base::Time::Now()));
  event_expecter.AddExpectedEventForPath(tmp_dir);
  delegate.RunUntilEventsMatch(event_expecter);
}

// Observe no change when a parent is modified.
TEST_F(FilePathWatcherTest, TrivialParentDirChange) {
  const base::FilePath tmp_dir = temp_dir_.GetPath();
  const base::FilePath sub_dir1 = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));
  const base::FilePath sub_dir2 =
      sub_dir1.Append(FILE_PATH_LITERAL("subdir_redux"));

  ASSERT_TRUE(CreateDirectory(sub_dir1));
  ASSERT_TRUE(CreateDirectory(sub_dir2));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(sub_dir2, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  // There should be no notification for a change to |sub_dir2|'s parent.
  ASSERT_TRUE(Move(sub_dir1, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  delegate.RunUntilEventsMatch(event_expecter);
}

// Do not crash when a directory is moved; https://crbug.com/1156603.
TEST_F(FilePathWatcherTest, TrivialDirMove) {
  const base::FilePath tmp_dir = temp_dir_.GetPath();
  const base::FilePath sub_dir = tmp_dir.Append(FILE_PATH_LITERAL("subdir"));

  ASSERT_TRUE(CreateDirectory(sub_dir));

  FilePathWatcher watcher;
  TestDelegate delegate;
  AccumulatingEventExpecter event_expecter;
  ASSERT_TRUE(SetupWatch(sub_dir, &watcher, &delegate,
                         FilePathWatcher::Type::kTrivial));

  ASSERT_TRUE(Move(sub_dir, tmp_dir.Append(FILE_PATH_LITERAL("over_here"))));
  event_expecter.AddExpectedEventForPath(sub_dir, /**error=*/true);
  delegate.RunUntilEventsMatch(event_expecter);
}

#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// TODO(crbug.com/40263777): Ideally most all of the tests above would be
// parameterized in this way.
class FilePathWatcherWithChangeInfoTest
    : public FilePathWatcherTest,
      public testing::WithParamInterface<
          std::tuple<FilePathWatcher::Type, bool>> {
 public:
  void SetUp() override { FilePathWatcherTest::SetUp(); }

 protected:
  FilePathWatcher::Type type() const { return std::get<0>(GetParam()); }
  bool report_modified_path() const { return std::get<1>(GetParam()); }

  FilePathWatcher::WatchOptions GetWatchOptions() const {
    return FilePathWatcher::WatchOptions{
        .type = type(), .report_modified_path = report_modified_path()};
  }
};

TEST_P(FilePathWatcherWithChangeInfoTest, NewFile) {
  // Each change should have these attributes.
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(test_file()), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(test_file()), HasNoMovedFromPath()));
#if BUILDFLAG(IS_MAC)
  static_assert(kExpectedEventsForNewFileWrite == 1);
  // Match the expected change types, in this order.
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated));
#else
  static_assert(kExpectedEventsForNewFileWrite == 2);
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated),
                           IsType(FilePathWatcher::ChangeType::kModified));
#endif

  // Put it all together.
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, NewDirectory) {
  const auto matcher = testing::ElementsAre(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsDirectory(),
      IsType(FilePathWatcher::ChangeType::kCreated),
      HasModifiedPath(test_file()), HasNoMovedFromPath()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(CreateDirectory(test_file()));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, ModifiedFile) {
  // TODO(crbug.com/40260973): Some platforms will not support
  // `ChangeType::kContentsModified`. Update this matcher once support for those
  // platforms is added.
  const auto matcher = ModifiedMatcher(test_file(), test_file());

  ASSERT_TRUE(WriteFile(test_file(), "content"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, MovedFile) {
  // TODO(crbug.com/40260973): Some platforms will not provide separate
  // events for "moved from" and "moved to". Update this matcher once support
  // for those platforms is added.
  // A moved file to the watched scope is considered "created", with respect
  // to the watched path.
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_file()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kCreated),
                     HasModifiedPath(test_file()), HasNoMovedFromPath()));

  base::FilePath source_file(temp_dir_.GetPath().AppendASCII("source"));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(Move(source_file, test_file()));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, DeletedFile) {
  ASSERT_TRUE(WriteFile(test_file(), "content"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(DeleteFile(test_file()));

  const auto matcher = testing::ElementsAre(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsDeletedFile(),
      IsType(FilePathWatcher::ChangeType::kDeleted),
      HasModifiedPath(test_file()), HasNoMovedFromPath()));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, DeletedDirectory) {
  ASSERT_TRUE(CreateDirectory(test_file()));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

#if BUILDFLAG(IS_WIN)
  // Windows doesn't allow the target directory to be deleted while there is a
  // FilePathWatcher watching it.
  ASSERT_FALSE(DeletePathRecursively(test_file()));
  delegate.SpinAndExpectNoEvents();
#else
  ASSERT_TRUE(DeletePathRecursively(test_file()));

  const auto matcher = testing::ElementsAre(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsDeletedDirectory(),
      IsType(FilePathWatcher::ChangeType::kDeleted),
      HasModifiedPath(test_file()), HasNoMovedFromPath()));
  delegate.RunUntilEventsMatch(matcher);
#endif
}

TEST_P(FilePathWatcherWithChangeInfoTest, MultipleWatchersSingleFile) {
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(test_file()), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(test_file()), HasNoMovedFromPath()));

#if BUILDFLAG(IS_MAC)
  static_assert(kExpectedEventsForNewFileWrite == 1);
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated));
#else
  static_assert(kExpectedEventsForNewFileWrite == 2);
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated),
                           IsType(FilePathWatcher::ChangeType::kModified));
#endif
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  FilePathWatcher watcher1, watcher2;
  TestDelegate delegate1, delegate2;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher1, &delegate1,
                                       GetWatchOptions()));
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher2, &delegate2,
                                       GetWatchOptions()));

  // Expect each delegate to get notified of all changes.
  ASSERT_TRUE(WriteFile(test_file(), "content"));

  delegate1.RunUntilEventsMatch(matcher);
  delegate2.RunUntilEventsMatch(matcher);
}

// TODO(b/358401685): FSEvents can sometimes coalesce the event flags from the
// two write operations in this test together, since the expected event flags
// for each event is very similar. When this happens, we receive one less event
// than expected, which results in a test flake / failure. Re-enable once this
// individual test flake is resolved.
#if !BUILDFLAG(IS_MAC)
TEST_P(FilePathWatcherWithChangeInfoTest, NonExistentDirectory) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file(dir.AppendASCII("file"));
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(file), testing::Not(HasErrored()), IsDeletedFile(),
                     HasModifiedPath(file), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified),
                             IsType(FilePathWatcher::ChangeType::kDeleted)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(file, &watcher, &delegate, GetWatchOptions()));

  // The delegate is only watching the file. Parent directory creation should
  // not trigger an event.
  ASSERT_TRUE(CreateDirectory(dir));

  // It may take some time for `watcher` to re-construct its watch list, so spin
  // for a bit while we ensure that creating the parent directory does not
  // trigger an event.
  delegate.RunUntilEventsMatch(testing::IsEmpty(),
                               ExpectedEventsSinceLastWait::kNone);

  ASSERT_TRUE(WriteFile(file, "content"));
  ASSERT_TRUE(WriteFile(file, "content v2"));
  ASSERT_TRUE(DeleteFile(file));
  delegate.RunUntilEventsMatch(matcher);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_P(FilePathWatcherWithChangeInfoTest, DirectoryChain) {
  base::FilePath path(temp_dir_.GetPath());
  std::vector<std::string> dir_names;
  for (int i = 0; i < 20; i++) {
    std::string dir(base::StringPrintf("d%d", i));
    dir_names.push_back(dir);
    path = path.AppendASCII(dir);
  }
  base::FilePath file(path.AppendASCII("file"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(file), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(file), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(file, &watcher, &delegate, GetWatchOptions()));

  base::FilePath sub_path(temp_dir_.GetPath());
  for (const auto& dir_name : dir_names) {
    sub_path = sub_path.AppendASCII(dir_name);
    ASSERT_TRUE(CreateDirectory(sub_path));
  }

  // Allow the watcher to reconstruct its watch list.
  SpinEventLoopForABit();

  ASSERT_TRUE(WriteFile(file, "content"));
  ASSERT_TRUE(WriteFile(file, "content v2"));
  delegate.RunUntilEventsMatch(matcher);
}

// Windows doesn't allow the target directory to be deleted while there is a
// FilePathWatcher watching it.
#if !BUILDFLAG(IS_WIN)
TEST_P(FilePathWatcherWithChangeInfoTest, DisappearingDirectory) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file(dir.AppendASCII("file"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(file), testing::Not(HasErrored()),
                     IsType(FilePathWatcher::ChangeType::kDeleted),
                     HasModifiedPath(file), HasNoMovedFromPath()));
  // TODO(crbug.com/40263766): inotify incorrectly reports an additional
  // deletion event for the parent directory (though while confusingly reporting
  // the path as `file`). Once fixed, update this matcher to assert that only
  // one event is received.
  const auto sequence_matcher = testing::Contains(IsDeletedFile());
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(file, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(DeletePathRecursively(dir));
  delegate.RunUntilEventsMatch(matcher);
}
#endif

TEST_P(FilePathWatcherWithChangeInfoTest, DeleteAndRecreate) {
#if BUILDFLAG(IS_MAC)
  static_assert(kExpectedEventsForNewFileWrite == 1);
  const auto each_event_matcher = testing::Each(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsDeletedFile(),
      HasModifiedPath(test_file()), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kDeleted),
                           IsType(FilePathWatcher::ChangeType::kCreated));
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);
#else
  static_assert(kExpectedEventsForNewFileWrite == 2);
  const auto each_event_matcher = testing::Each(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsDeletedFile(),
      HasModifiedPath(test_file()), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kDeleted),
                           IsType(FilePathWatcher::ChangeType::kCreated),
                           IsType(FilePathWatcher::ChangeType::kModified));
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);
#endif

  ASSERT_TRUE(WriteFile(test_file(), "content"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(DeleteFile(test_file()));
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  delegate.RunUntilEventsMatch(matcher);
}

// TODO(371594111): Tests are flaky on Macos
#if BUILDFLAG(IS_MAC)
#define MAYBE_WatchDirectory DISABLED_WatchDirectory
#else
#define MAYBE_WatchDirectory WatchDirectory
#endif
TEST_P(FilePathWatcherWithChangeInfoTest, MAYBE_WatchDirectory) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath file1(dir.AppendASCII("file1"));
  base::FilePath file2(dir.AppendASCII("file2"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(testing::Not(HasErrored()), HasNoMovedFromPath()));
  const auto sequence_matcher = testing::IsSupersetOf(
      {testing::AllOf(HasPath(report_modified_path() ? file1 : dir),
                      IsDeletedFile(),
                      IsType(FilePathWatcher::ChangeType::kCreated),
                      HasModifiedPath(file1)),
       testing::AllOf(HasPath(report_modified_path() ? file1 : dir),
                      IsDeletedFile(),
                      IsType(FilePathWatcher::ChangeType::kModified),
                      HasModifiedPath(file1)),
       testing::AllOf(HasPath(report_modified_path() ? file1 : dir),
                      IsDeletedFile(),
                      IsType(FilePathWatcher::ChangeType::kDeleted),
                      HasModifiedPath(file1)),
       testing::AllOf(HasPath(report_modified_path() ? file2 : dir),
                      IsDeletedFile(),
                      IsType(FilePathWatcher::ChangeType::kCreated),
                      HasModifiedPath(file2))});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(dir));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file
  // system notifications created while setting up the file system for this
  // test. Spin the event loop to ensure that the events have been processed
  // by the time the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(dir, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(WriteFile(file1, "content"));
  ASSERT_TRUE(WriteFile(file1, "content v2"));

  // TODO(b/358401685): On Mac, a minimum of two additional event loop spins are
  // required to prevent the flags from the previous `WriteFile` from being
  // automatically coalesced into the following `DeleteFile` event, by FSEvents.
  // Determine if there's a way to avoid adding these extra spins.
#if BUILDFLAG(IS_MAC)
  SpinEventLoopForABit();
  SpinEventLoopForABit();
#endif

  ASSERT_TRUE(DeleteFile(file1));
  ASSERT_TRUE(WriteFile(file2, "content"));
  delegate.RunUntilEventsMatch(matcher);
}

// TODO(crbug.com/362715979): This test is disabled on Mac due to unexpected,
// test-specific behavior - we receive no FSEvents events when the ancestor dir
// of the watched target is moved out-of-scope. Since we never receive events
// from FSEvents, it's impossible for us to report the expected 'delete' event.
// This behavior does not repro in manual testing. In manual tests of this use
// case, we receive all events as expected, including the equivalent 'delete'
// event that never arrives in the unittest.
#if !BUILDFLAG(IS_MAC)
TEST_P(FilePathWatcherWithChangeInfoTest, MoveParent) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath dest(temp_dir_.GetPath().AppendASCII("dest"));
  base::FilePath subdir(dir.AppendASCII("subdir"));
  base::FilePath file(subdir.AppendASCII("file"));

  const auto each_event_matcher = testing::Each(testing::Not(HasErrored()));
  // TODO(crbug.com/40263766): inotify incorrectly sometimes reports
  // the first event as a directory creation... why?
  // A moved file to the watched scope is considered "created", with respect
  // to the watched path.
  const auto file_delegate_sequence_matcher = testing::IsSupersetOf(
      {testing::AllOf(HasPath(file), IsFile(),
                      IsType(FilePathWatcher::ChangeType::kCreated),
                      HasModifiedPath(file), HasNoMovedFromPath()),
       testing::AllOf(HasPath(file), IsMovedFile(),
                      IsType(FilePathWatcher::ChangeType::kDeleted),
                      HasModifiedPath(file), HasNoMovedFromPath())});
  const auto subdir_delegate_sequence_matcher = testing::IsSupersetOf(
      {testing::AllOf(HasPath(subdir), IsDirectory(),
                      IsType(FilePathWatcher::ChangeType::kCreated),
                      HasModifiedPath(subdir), HasNoMovedFromPath()),
       testing::AllOf(HasPath(report_modified_path() ? file : subdir), IsFile(),
                      IsType(FilePathWatcher::ChangeType::kCreated),
                      HasModifiedPath(file), HasNoMovedFromPath()),
       testing::AllOf(HasPath(subdir), IsDirectory(),
                      IsType(FilePathWatcher::ChangeType::kDeleted),
                      HasModifiedPath(subdir), HasNoMovedFromPath())});
  const auto file_delegate_matcher =
      testing::AllOf(each_event_matcher, file_delegate_sequence_matcher);
  const auto subdir_delegate_matcher =
      testing::AllOf(each_event_matcher, subdir_delegate_sequence_matcher);

  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(file, &file_watcher, &file_delegate,
                                       GetWatchOptions()));
  ASSERT_TRUE(SetupWatchWithChangeInfo(subdir, &subdir_watcher,
                                       &subdir_delegate, GetWatchOptions()));

  // Setup a directory hierarchy.
  // We should only get notified on `subdir_delegate` of its creation.
  ASSERT_TRUE(CreateDirectory(subdir));
  // Allow the watchers to reconstruct their watch lists.
  SpinEventLoopForABit();

  ASSERT_TRUE(WriteFile(file, "content"));
  // Allow the file watcher to reconstruct its watch list.
  SpinEventLoopForABit();

  Move(dir, dest);
  // dir/subdir/file -> dest/subdir/file
  file_delegate.RunUntilEventsMatch(file_delegate_matcher);
  subdir_delegate.RunUntilEventsMatch(subdir_delegate_matcher);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_P(FilePathWatcherWithChangeInfoTest, MoveChild) {
  base::FilePath source_dir(temp_dir_.GetPath().AppendASCII("source"));
  base::FilePath source_subdir(source_dir.AppendASCII("subdir"));
  base::FilePath source_file(source_subdir.AppendASCII("file"));
  base::FilePath dest_dir(temp_dir_.GetPath().AppendASCII("dest"));
  base::FilePath dest_subdir(dest_dir.AppendASCII("subdir"));
  base::FilePath dest_file(dest_subdir.AppendASCII("file"));

  // A moved file to the watched scope is considered "created", with respect
  // to the watched path.
  const auto each_event_matcher = testing::Each(testing::AllOf(
      testing::Not(HasErrored()), IsType(FilePathWatcher::ChangeType::kCreated),
      HasNoMovedFromPath()));
#if BUILDFLAG(IS_MAC)
  // Events for changes on the root path are always reported as 'unknown' by
  // FSEvents.
  const auto file_delegate_sequence_matcher =
      testing::ElementsAre(testing::AllOf(
          HasPath(dest_file), IsUnknownPathType(), HasModifiedPath(dest_file)));
  const auto subdir_delegate_sequence_matcher = testing::ElementsAre(
      testing::AllOf(HasPath(dest_subdir), IsUnknownPathType(),
                     HasModifiedPath(dest_subdir)));
#else
  const auto file_delegate_sequence_matcher =
      testing::ElementsAre(testing::AllOf(HasPath(dest_file), IsMovedFile(),
                                          HasModifiedPath(dest_file)));
  const auto subdir_delegate_sequence_matcher =
      testing::ElementsAre(testing::AllOf(HasPath(dest_subdir), IsDirectory(),
                                          HasModifiedPath(dest_subdir)));
#endif
  const auto file_delegate_matcher =
      testing::AllOf(each_event_matcher, file_delegate_sequence_matcher);
  const auto subdir_delegate_matcher =
      testing::AllOf(each_event_matcher, subdir_delegate_sequence_matcher);

  // Setup a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(source_subdir));
  ASSERT_TRUE(WriteFile(source_file, "content"));

  FilePathWatcher file_watcher, subdir_watcher;
  TestDelegate file_delegate, subdir_delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(dest_file, &file_watcher, &file_delegate,
                                       GetWatchOptions()));
  ASSERT_TRUE(SetupWatchWithChangeInfo(dest_subdir, &subdir_watcher,
                                       &subdir_delegate, GetWatchOptions()));

  // Move the directory into place, s.t. the watched file appears.
  ASSERT_TRUE(Move(source_dir, dest_dir));

  file_delegate.RunUntilEventsMatch(file_delegate_matcher);
  subdir_delegate.RunUntilEventsMatch(subdir_delegate_matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, MoveChildWithinWatchedScope) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath src_file(dir.AppendASCII("src_file"));
  base::FilePath dest_file(dir.AppendASCII("dest_file"));

  const auto each_event_matcher =
      testing::Each(testing::AllOf(testing::Not(HasErrored()), IsFile()));

  // In most cases, the first item in this set should match, as one coalesced
  // move event. Since coalescing is not guaranteed, we should also expect two
  // separate move events being reported.
  const auto coalesced_move_event_sequence_matcher = testing::ElementsAre(
      testing::AllOf(HasPath(report_modified_path() ? dest_file : dir),
                     IsType(FilePathWatcher::ChangeType::kMoved),
                     HasModifiedPath(dest_file), HasMovedFromPath(src_file)));

  // Separate move events will be considered as created or deleted, with
  // respect to the watched scope.
  const auto separate_move_events_sequence_matcher = testing::ElementsAre(
      testing::AllOf(HasPath(report_modified_path() ? src_file : dir),
                     IsType(FilePathWatcher::ChangeType::kDeleted),
                     HasModifiedPath(src_file), HasNoMovedFromPath()),
      testing::AllOf(HasPath(report_modified_path() ? dest_file : dir),
                     IsType(FilePathWatcher::ChangeType::kCreated),
                     HasModifiedPath(dest_file), HasNoMovedFromPath()));
  const auto delegate_matcher =
      testing::AllOf(each_event_matcher,
                     testing::AnyOf(coalesced_move_event_sequence_matcher,
                                    separate_move_events_sequence_matcher));

  // Set up a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(src_file, "content"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(dir, &watcher, &delegate, GetWatchOptions()));

  // Moving dir/src_file to dir/dest_file should trigger a move event for
  // dir watcher, with both old and new file paths present.
  ASSERT_TRUE(Move(src_file, dest_file));
  delegate.RunUntilEventsMatch(delegate_matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, MoveChildOutOrIntoWatchedScope) {
  base::FilePath foo_dir(temp_dir_.GetPath().AppendASCII("foo"));
  base::FilePath foo_subdir(foo_dir.AppendASCII("foo_subdir"));
  base::FilePath bar_dir(temp_dir_.GetPath().AppendASCII("bar"));
  base::FilePath bar_subdir(bar_dir.AppendASCII("bar_subdir"));

  const auto each_event_matcher = testing::Each(testing::Not(HasErrored()));
  // A moved file from/to the wathced scope is considered "deleted" / "created",
  // with respect to the watched path.
  const auto foo_delegate_sequence_matcher =
      testing::ElementsAre(testing::AllOf(
          HasPath(report_modified_path() ? foo_subdir : foo_dir),
          IsDeletedDirectory(), IsType(FilePathWatcher::ChangeType::kDeleted),
          HasModifiedPath(foo_subdir), HasNoMovedFromPath()));
  const auto bar_delegate_sequence_matcher =
      testing::ElementsAre(testing::AllOf(
          HasPath(report_modified_path() ? bar_subdir : bar_dir), IsDirectory(),
          IsType(FilePathWatcher::ChangeType::kCreated),
          HasModifiedPath(bar_subdir), HasNoMovedFromPath()));
  const auto foo_delegate_matcher =
      testing::AllOf(each_event_matcher, foo_delegate_sequence_matcher);
  const auto bar_delegate_matcher =
      testing::AllOf(each_event_matcher, bar_delegate_sequence_matcher);

  // Set up a directory hierarchy.
  ASSERT_TRUE(CreateDirectory(foo_subdir));
  ASSERT_TRUE(CreateDirectory(bar_dir));

  FilePathWatcher foo_watcher, bar_watcher;
  TestDelegate foo_delegate, bar_delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(foo_dir, &foo_watcher, &foo_delegate,
                                       GetWatchOptions()));
  ASSERT_TRUE(SetupWatchWithChangeInfo(bar_dir, &bar_watcher, &bar_delegate,
                                       GetWatchOptions()));

  // Moving foo/foo_subdir to bar/bar_subdir should trigger a `kDeleted` event
  // for foo_dir watcher with the old file path present (since it is moving
  // out of its watched scope), and a `kCreated` event for bar_dir watcher with
  // the new file path present (since it is moving into its watched scope).
  ASSERT_TRUE(Move(foo_subdir, bar_subdir));
  foo_delegate.RunUntilEventsMatch(foo_delegate_matcher);
  bar_delegate.RunUntilEventsMatch(bar_delegate_matcher);
}

// TODO(pauljensen): Re-enable when crbug.com/475568 is fixed and SetUp() places
// the |temp_dir_| in /data.
#if !BUILDFLAG(IS_ANDROID)

// This test is disabled on Mac because we don't support reporting file metadata
// changes on FSEvents.
#if !BUILDFLAG(IS_MAC)
TEST_P(FilePathWatcherWithChangeInfoTest, NoEventWhenFileAttributesChanged) {
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_file()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kModified),
                     HasModifiedPath(test_file()), HasNoMovedFromPath()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(MakeFileUnreadable(test_file()));
  delegate.SpinAndExpectNoEvents();
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_P(FilePathWatcherWithChangeInfoTest, CreateLink) {
  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_link()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kCreated),
                     HasModifiedPath(test_link()), HasNoMovedFromPath()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_link(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the link is created.
  // Note that test_file() doesn't have to exist.
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));
  delegate.RunUntilEventsMatch(matcher);
}

// Unfortunately this test case only works if the link target exists.
// TODO(craig) fix this as part of crbug.com/91561.
TEST_P(FilePathWatcherWithChangeInfoTest, DeleteLink) {
  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_link()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kDeleted),
                     HasModifiedPath(test_link()), HasNoMovedFromPath()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_link(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the link is deleted.
  ASSERT_TRUE(DeleteFile(test_link()));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, ModifiedLinkedFile) {
  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_link()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kModified),
                     HasModifiedPath(test_link()), HasNoMovedFromPath()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_link(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the file is modified.
  ASSERT_TRUE(WriteFile(test_file(), "new content"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, CreateTargetLinkedFile) {
  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(test_link()), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(test_link()), HasNoMovedFromPath()));
  // TODO(crbug.com/40260973): Update this when change types are
  // supported on on more platforms.
  static_assert(kExpectedEventsForNewFileWrite == 2);
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated),
                           IsType(FilePathWatcher::ChangeType::kModified));
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_link(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the target file is created.
  ASSERT_TRUE(WriteFile(test_file(), "content"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, DeleteTargetLinkedFile) {
  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto matcher = testing::ElementsAre(
      testing::AllOf(HasPath(test_link()), testing::Not(HasErrored()), IsFile(),
                     IsType(FilePathWatcher::ChangeType::kDeleted),
                     HasModifiedPath(test_link()), HasNoMovedFromPath()));

  ASSERT_TRUE(WriteFile(test_file(), "content"));
  ASSERT_TRUE(CreateSymbolicLink(test_file(), test_link()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(SetupWatchWithChangeInfo(test_link(), &watcher, &delegate,
                                       GetWatchOptions()));

  // Now make sure we get notified if the target file is deleted.
  ASSERT_TRUE(DeleteFile(test_file()));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, LinkedDirectoryPart1) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));

  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(linkfile), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(linkfile), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified),
                             IsType(FilePathWatcher::ChangeType::kDeleted)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  // dir/file should exist.
  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(WriteFile(file, "content"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  // Note that we are watching dir.lnk/file which doesn't exist yet.
  ASSERT_TRUE(SetupWatchWithChangeInfo(linkfile, &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));
  // Allow the watcher to reconstruct its watch list.
  SpinEventLoopForABit();

  ASSERT_TRUE(WriteFile(file, "content v2"));
  ASSERT_TRUE(WriteFile(file, "content v2"));
  ASSERT_TRUE(DeleteFile(file));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, LinkedDirectoryPart2) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));

  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(linkfile), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(linkfile), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified),
                             IsType(FilePathWatcher::ChangeType::kDeleted)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  // Now create the link from dir.lnk pointing to dir but
  // neither dir nor dir/file exist yet.
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));

  FilePathWatcher watcher;
  TestDelegate delegate;
  // Note that we are watching dir.lnk/file.
  ASSERT_TRUE(SetupWatchWithChangeInfo(linkfile, &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(CreateDirectory(dir));
  // Allow the watcher to reconstruct its watch list.
  SpinEventLoopForABit();

  ASSERT_TRUE(WriteFile(file, "content"));
  ASSERT_TRUE(WriteFile(file, "content v2"));
  ASSERT_TRUE(DeleteFile(file));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, LinkedDirectoryPart3) {
  base::FilePath dir(temp_dir_.GetPath().AppendASCII("dir"));
  base::FilePath link_dir(temp_dir_.GetPath().AppendASCII("dir.lnk"));
  base::FilePath file(dir.AppendASCII("file"));
  base::FilePath linkfile(link_dir.AppendASCII("file"));

  // TODO(crbug.com/40260973): Check for symlink-ness on platforms which
  // support it.
  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(linkfile), testing::Not(HasErrored()), IsFile(),
                     HasModifiedPath(linkfile), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified),
                             IsType(FilePathWatcher::ChangeType::kDeleted)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(dir));
  ASSERT_TRUE(CreateSymbolicLink(dir, link_dir));

  FilePathWatcher watcher;
  TestDelegate delegate;
  // Note that we are watching dir.lnk/file but the file doesn't exist yet.
  ASSERT_TRUE(SetupWatchWithChangeInfo(linkfile, &watcher, &delegate,
                                       GetWatchOptions()));

  ASSERT_TRUE(WriteFile(file, "content"));
  ASSERT_TRUE(WriteFile(file, "content v2"));
  ASSERT_TRUE(DeleteFile(file));
  delegate.RunUntilEventsMatch(matcher);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_P(FilePathWatcherWithChangeInfoTest, CreatedFileInDirectory) {
  // Expect the change to be reported as a file creation, not as a
  // directory modification.
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));

  const auto matcher = testing::IsSupersetOf({testing::AllOf(
      HasPath(report_modified_path() ? child : parent), IsFile(),
      IsType(FilePathWatcher::ChangeType::kCreated), testing::Not(HasErrored()),
      HasModifiedPath(child), HasNoMovedFromPath())});
  ASSERT_TRUE(CreateDirectory(parent));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(WriteFile(child, "contents"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, ModifiedFileInDirectory) {
  // Expect the change to be reported as a file modification, not as a
  // directory modification.
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));

  const auto matcher =
      ModifiedMatcher(report_modified_path() ? child : parent, child);

  ASSERT_TRUE(CreateDirectory(parent));
  ASSERT_TRUE(WriteFile(child, "contents"));
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40286767): There appears to be a race condition
  // between setting up the inotify watch and the processing of the file system
  // notifications created while setting up the file system for this test. Spin
  // the event loop to ensure that the events have been processed by the time
  // the inotify watch has been set up.
  SpinEventLoopForABit();
#endif  // BUILDFLAG(IS_ANDROID)

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(WriteFile(child, "contents v2"));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, DeletedFileInDirectory) {
  // Expect the change to be reported as a file deletion, not as a
  // directory modification.
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));
  const auto matcher = testing::ElementsAre(testing::AllOf(
      HasPath(report_modified_path() ? child : parent), IsDeletedFile(),
      IsType(FilePathWatcher::ChangeType::kDeleted), testing::Not(HasErrored()),
      HasModifiedPath(child), HasNoMovedFromPath()));

  ASSERT_TRUE(CreateDirectory(parent));
  ASSERT_TRUE(WriteFile(child, "contents"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(DeleteFile(child));
  delegate.RunUntilEventsMatch(matcher);
}

// TODO(371594111): Tests are flaky on Macos
#if BUILDFLAG(IS_MAC)
#define MAYBE_FileInDirectory DISABLED_FileInDirectory
#else
#define MAYBE_FileInDirectory FileInDirectory
#endif
TEST_P(FilePathWatcherWithChangeInfoTest, MAYBE_FileInDirectory) {
  // Expect the changes to be reported as events on the file, not as
  // modifications to the directory.
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(report_modified_path() ? child : parent),
                     testing::Not(HasErrored()), IsDeletedFile(),
                     HasModifiedPath(child), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::IsSupersetOf({IsType(FilePathWatcher::ChangeType::kCreated),
                             IsType(FilePathWatcher::ChangeType::kModified),
                             IsType(FilePathWatcher::ChangeType::kDeleted)});
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(parent));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(WriteFile(child, "contents"));
  ASSERT_TRUE(WriteFile(child, "contents v2"));

// TODO(b/358401685): On Mac, a minimum of two additional event loop spins are
// required to prevent the flags from the previous `WriteFile` from being
// automatically coalesced into the following `DeleteFile` event, by FSEvents.
// Determine if there's a way to avoid adding these extra spins.
#if BUILDFLAG(IS_MAC)
  SpinEventLoopForABit();
  SpinEventLoopForABit();
#endif

  ASSERT_TRUE(DeleteFile(child));
  delegate.RunUntilEventsMatch(matcher);
}

TEST_P(FilePathWatcherWithChangeInfoTest, DirectoryInDirectory) {
  // Expect the changes to be reported as events on the child directory, not as
  // modifications to the parent directory.
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(HasPath(report_modified_path() ? child : parent),
                     testing::Not(HasErrored()), IsDeletedDirectory(),
                     HasModifiedPath(child), HasNoMovedFromPath()));
  const auto sequence_matcher =
      testing::ElementsAre(IsType(FilePathWatcher::ChangeType::kCreated),
                           IsType(FilePathWatcher::ChangeType::kDeleted));
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(parent));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(CreateDirectory(child));
  ASSERT_TRUE(DeletePathRecursively(child));
  delegate.RunUntilEventsMatch(matcher);
}

// TODO(crbug.com/368982619): Disable due to flakiness.
#if BUILDFLAG(IS_MAC)
#define MAYBE_NestedDirectoryInDirectory DISABLED_NestedDirectoryInDirectory
#else
#define MAYBE_NestedDirectoryInDirectory NestedDirectoryInDirectory
#endif  // BUILDFLAG(IS_MAC)
TEST_P(FilePathWatcherWithChangeInfoTest, MAYBE_NestedDirectoryInDirectory) {
  base::FilePath parent(temp_dir_.GetPath().AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));
  base::FilePath grandchild(child.AppendASCII("grandchild"));

  const auto each_event_matcher = testing::Each(
      testing::AllOf(testing::Not(HasErrored()), HasNoMovedFromPath()));

  EventListMatcher sequence_matcher;

  auto reported_child_path_created_matcher = testing::AllOf(
      HasPath(report_modified_path() ? child : parent), IsDeletedDirectory(),
      HasModifiedPath(child), IsType(FilePathWatcher::ChangeType::kCreated));
  auto reported_child_path_deleted_matcher = testing::AllOf(
      HasPath(report_modified_path() ? child : parent), IsDeletedDirectory(),
      HasModifiedPath(child), IsType(FilePathWatcher::ChangeType::kDeleted));
  if (type() == FilePathWatcher::Type::kRecursive) {
    sequence_matcher = testing::IsSupersetOf(
        {reported_child_path_created_matcher,
         testing::AllOf(HasPath(report_modified_path() ? grandchild : parent),
                        IsDeletedFile(), HasModifiedPath(grandchild),
                        IsType(FilePathWatcher::ChangeType::kCreated)),
         testing::AllOf(HasPath(report_modified_path() ? grandchild : parent),
                        IsDeletedFile(), HasModifiedPath(grandchild),
                        IsType(FilePathWatcher::ChangeType::kModified)),
         testing::AllOf(HasPath(report_modified_path() ? grandchild : parent),
                        IsDeletedFile(), HasModifiedPath(grandchild),
                        IsType(FilePathWatcher::ChangeType::kDeleted)),
         reported_child_path_deleted_matcher});
  } else {
    // Do not expect changes to `grandchild` when watching `parent`
    // non-recursively.
#if BUILDFLAG(IS_WIN)
    // Modified events on directories may or may not get filtered because the
    // directories get deleted too fast before we can see they're directories.
    sequence_matcher =
        testing::IsSupersetOf({reported_child_path_created_matcher,
                               reported_child_path_deleted_matcher});
#else
    sequence_matcher =
        testing::ElementsAre(reported_child_path_created_matcher,
                             reported_child_path_deleted_matcher);
#endif
  }
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(parent));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(CreateDirectory(child));
  // Allow the watcher to reconstruct its watch list.
  SpinEventLoopForABit();

  ASSERT_TRUE(WriteFile(grandchild, "contents"));

// TODO(b/358401685): On Mac, a minimum of one additional event loop spin is
// required to prevent the flags from the previous `WriteFile` from being
// automatically coalesced into the following `WriteFile` event, by FSEvents.
// Determine if there's a way to avoid adding these extra spins.
#if BUILDFLAG(IS_MAC)
  SpinEventLoopForABit();
#endif

  ASSERT_TRUE(WriteFile(grandchild, "contents v2"));

// TODO(b/358401685): On Mac, a minimum of two additional event loop spins are
// required to prevent the flags from the previous `WriteFile` from being
// automatically coalesced into the following `DeleteFile` event, by FSEvents.
// Determine if there's a way to avoid adding these extra spins.
#if BUILDFLAG(IS_MAC)
  SpinEventLoopForABit();
  SpinEventLoopForABit();
#endif

  ASSERT_TRUE(DeleteFile(grandchild));
  ASSERT_TRUE(DeletePathRecursively(child));
  delegate.RunUntilEventsMatch(matcher);
}

// Windows doesn't allow the target directory to be deleted while there is a
// FilePathWatcher watching it.
#if !BUILDFLAG(IS_WIN)
TEST_P(FilePathWatcherWithChangeInfoTest, DeleteDirectoryRecursively) {
  base::FilePath grandparent(temp_dir_.GetPath());
  base::FilePath parent(grandparent.AppendASCII("parent"));
  base::FilePath child(parent.AppendASCII("child"));
  base::FilePath grandchild(child.AppendASCII("grandchild"));

  const auto each_event_matcher = testing::Each(testing::AllOf(
      testing::Not(HasErrored()), IsType(FilePathWatcher::ChangeType::kDeleted),
      HasNoMovedFromPath()));

#if BUILDFLAG(IS_MAC)
  // Mac can only guarantee that the watched directory will be reported as
  // deleted.
  EventListMatcher sequence_matcher = testing::IsSupersetOf(
      {testing::AllOf(HasPath(parent), IsDeletedDirectory())});
#else
  // TODO(crbug.com/40263766): inotify incorrectly reports an additional
  // deletion event. Once fixed, update this matcher to assert that only one
  // event per removed file/dir is received.
  EventListMatcher sequence_matcher;
  if (type() == FilePathWatcher::Type::kRecursive) {
    sequence_matcher = testing::IsSupersetOf(
        {testing::AllOf(HasPath(parent), IsDirectory(),
                        HasModifiedPath(parent)),
         testing::AllOf(HasPath(report_modified_path() ? child : parent),
                        IsDirectory(), HasModifiedPath(child)),
         testing::AllOf(HasPath(report_modified_path() ? grandchild : parent),
                        IsFile(), HasModifiedPath(grandchild))});
  } else {
    // Do not expect changes to `grandchild` when watching `parent`
    // non-recursively.
    sequence_matcher = testing::IsSupersetOf(
        {testing::AllOf(HasPath(parent), IsDirectory(),
                        HasModifiedPath(parent)),
         testing::AllOf(HasPath(report_modified_path() ? child : parent),
                        IsDirectory(), HasModifiedPath(child))});
  }
#endif  // BUILDFLAG(IS_MAC)
  const auto matcher = testing::AllOf(each_event_matcher, sequence_matcher);

  ASSERT_TRUE(CreateDirectory(parent));
  ASSERT_TRUE(CreateDirectory(child));
  ASSERT_TRUE(WriteFile(grandchild, "contents"));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(parent, &watcher, &delegate, GetWatchOptions()));

  ASSERT_TRUE(DeletePathRecursively(grandparent));
  delegate.RunUntilEventsMatch(matcher);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    FilePathWatcherWithChangeInfoTest,
    ::testing::Combine(::testing::Values(FilePathWatcher::Type::kNonRecursive,
                                         FilePathWatcher::Type::kRecursive),
                       // Is WatchOptions.report_modified_path enabled?
                       ::testing::Bool()));

#else

TEST_F(FilePathWatcherTest, UseDummyChangeInfoIfNotSupported) {
  const auto matcher = testing::ElementsAre(testing::AllOf(
      HasPath(test_file()), testing::Not(HasErrored()), IsUnknownPathType(),
      IsType(FilePathWatcher::ChangeType::kUnknown),
      HasModifiedPath(base::FilePath()), HasNoMovedFromPath()));

  FilePathWatcher watcher;
  TestDelegate delegate;
  ASSERT_TRUE(
      SetupWatchWithChangeInfo(test_file(), &watcher, &delegate,
                               {.type = FilePathWatcher::Type::kNonRecursive}));

  ASSERT_TRUE(CreateDirectory(test_file()));
  delegate.RunUntilEventsMatch(matcher);
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace content
