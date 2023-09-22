// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"

#include <list>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/buildflag.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_system_access_change_source.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/test/test_web_contents.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using Change = FileSystemAccessWatcherManager::Observation::Change;
using Observation = FileSystemAccessWatcherManager::Observation;

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
void SpinEventLoopForABit() {
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), TestTimeouts::tiny_timeout());
  loop.Run();
}

// TODO(https://crbug.com/1425601): Report the modified path on more platforms.
bool ReportsModifiedPathForLocalObservations() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

// Accumulates changes it receives from the given `observation`.
class ChangeAccumulator {
 public:
  explicit ChangeAccumulator(std::unique_ptr<Observation> observation)
      : observation_(std::move(observation)) {
    observation_->SetCallback(base::BindRepeating(&ChangeAccumulator::OnChanges,
                                                  weak_factory_.GetWeakPtr()));
  }
  ChangeAccumulator(const ChangeAccumulator&) = delete;
  ChangeAccumulator& operator=(const ChangeAccumulator&) = delete;
  ~ChangeAccumulator() = default;

  void OnChanges(const std::list<Change>& changes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& change : changes) {
      received_changes_.push_back(change);
    }
  }

  Observation* observation() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return observation_.get();
  }

  const std::list<Change>& changes() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return received_changes_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Observation> observation_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::list<Change> received_changes_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ChangeAccumulator> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// Trivial implementation of a change source which allows tests to signal
// changes.
class FakeChangeSource : public FileSystemAccessChangeSource {
 public:
  explicit FakeChangeSource(FileSystemAccessWatchScope scope)
      : FileSystemAccessChangeSource(std::move(scope)) {}
  FakeChangeSource(const FakeChangeSource&) = delete;
  FakeChangeSource& operator=(const FakeChangeSource&) = delete;
  ~FakeChangeSource() override = default;

  // FileSystemAccessChangeSource:
  void Initialize(base::OnceCallback<void(bool)> on_initialized) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(on_initialized).Run(initialization_result_);
  }

  void Signal(base::FilePath relative_path, bool error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NotifyOfChange(std::move(relative_path), error);
  }

  void set_initialization_result(bool result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    initialization_result_ = result;
  }

 private:
  bool initialization_result_ GUARDED_BY_CONTEXT(sequence_checker_) = true;
};

}  // namespace

class FileSystemAccessWatcherManagerTest : public testing::Test {
 public:
  FileSystemAccessWatcherManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(dir_.Set(base::MakeLongFilePath(dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)

    web_contents_ = web_contents_factory_.CreateWebContents(&browser_context_);
    static_cast<TestWebContents*>(web_contents_)->NavigateAndCommit(kTestUrl);

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

  void TearDown() override {
    manager_.reset();
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(dir_.Delete());
  }

  FileSystemAccessWatcherManager& watcher_manager() const {
    return manager_->watcher_manager();
  }

 protected:
  const GURL kTestUrl = GURL("http://example.com/foo");

  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;

  TestBrowserContext browser_context_;
  TestWebContentsFactory web_contents_factory_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  raw_ptr<WebContents> web_contents_;
};

// Watching the local file system is not supported on Android or Fuchsia.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
TEST_F(FileSystemAccessWatcherManagerTest, BasicRegistration) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);

  EXPECT_FALSE(watcher_manager().HasObservationsForTesting());
  EXPECT_FALSE(watcher_manager().HasSourcesForTesting());

  {
    base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
    watcher_manager().GetDirectoryObservation(
        dir_url,
        /*is_recursive=*/false, get_observation_future.GetCallback());
    ASSERT_TRUE(get_observation_future.Get());

    // An observation should have been created.
    auto observation = get_observation_future.Take();
    EXPECT_TRUE(watcher_manager().HasObservationForTesting(observation.get()));

    // A source should have been created to cover the scope of the observation.
    EXPECT_TRUE(watcher_manager().HasSourcesForTesting());
  }

  // Destroying an observation unregisters it with the manager and removes the
  // respective source.
  EXPECT_FALSE(watcher_manager().HasObservationsForTesting());
  EXPECT_FALSE(watcher_manager().HasSourcesForTesting());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

TEST_F(FileSystemAccessWatcherManagerTest, BasicRegistrationUnownedSource) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  {
    FakeChangeSource source(
        FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
    watcher_manager().RegisterSource(&source);
    EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));
  }

  // Destroying a source unregisters it with the manager.
  EXPECT_FALSE(watcher_manager().HasSourcesForTesting());
}

TEST_F(FileSystemAccessWatcherManagerTest, UnownedSource) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  // Attempting to observe a scope covered by `source` will use `source`.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  source.Signal(/*relative_path=*/base::FilePath(), /*error=*/false);

  std::list<Change> expected_changes = {{file_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
}

TEST_F(FileSystemAccessWatcherManagerTest, SourceFailsInitialization) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  source.set_initialization_result(false);

  // Attempting to observe a scope covered by `source` will use `source`.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  EXPECT_FALSE(get_observation_future.Get());

  // TODO(https://crbug.com/1019297): Determine what should happen on failure to
  // initialize a source, then add better test coverage.
}

TEST_F(FileSystemAccessWatcherManagerTest, RemoveObservation) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  // Attempting to observe a scope covered by `source` will use `source`.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  ASSERT_TRUE(get_observation_future.Get());

  {
    ChangeAccumulator accumulator(get_observation_future.Take());
    EXPECT_TRUE(
        watcher_manager().HasObservationForTesting(accumulator.observation()));

    source.Signal(/*relative_path=*/base::FilePath(), /*error=*/false);

    std::list<Change> expected_changes = {{file_url, /*error=*/false}};
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return testing::Matches(testing::ContainerEq(expected_changes))(
          accumulator.changes());
    }));
  }

  // Signaling changes after the observation was removed should not crash.
  source.Signal(/*relative_path=*/base::FilePath(), /*error=*/false);
  EXPECT_FALSE(watcher_manager().HasObservationsForTesting());
}

TEST_F(FileSystemAccessWatcherManagerTest, UnsupportedScope) {
  // TODO(https://crbug.com/1019297): Sandboxed backends are not yet supported.
  auto temporary_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:http://chromium.org/temporary/i/has/a.bucket"));

  // Attempting to observe the given file will fail.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(temporary_url,
                                       get_observation_future.GetCallback());
  EXPECT_FALSE(get_observation_future.Get());
}

// TODO(https://crbug.com/1019297): Add tests covering more edge cases regarding
// overlapping scopes.
TEST_F(FileSystemAccessWatcherManagerTest, OverlappingSourceScopes) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);
  base::FilePath file_path = dir_path.AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source_for_file(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
  watcher_manager().RegisterSource(&source_for_file);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source_for_file));

  // Add another source which covers the scope of `source_for_file`, and more.
  FakeChangeSource source_for_dir(
      FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
          dir_url, /*is_recursive=*/true));
  watcher_manager().RegisterSource(&source_for_dir);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source_for_dir));

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  source_for_file.Signal(/*relative_path=*/base::FilePath(), /*error=*/false);
  source_for_dir.Signal(/*relative_path=*/file_path.BaseName(),
                        /*error=*/false);

  // TODO(https://crbug.com/1019297): It would be nice if the watcher manager
  // could consolidate these changes....

  std::list<Change> expected_changes = {{file_url, /*error=*/false},
                                        {file_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
}

TEST_F(FileSystemAccessWatcherManagerTest, OverlappingObservationScopes) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);
  base::FilePath file_path = dir_path.AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source(FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      dir_url, /*is_recursive=*/true));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  base::test::TestFuture<std::unique_ptr<Observation>>
      get_dir_observation_future;
  watcher_manager().GetDirectoryObservation(
      dir_url, /*is_recursive=*/true, get_dir_observation_future.GetCallback());
  EXPECT_TRUE(get_dir_observation_future.Get());

  ChangeAccumulator dir_accumulator(get_dir_observation_future.Take());
  EXPECT_TRUE(watcher_manager().HasObservationForTesting(
      dir_accumulator.observation()));

  base::test::TestFuture<std::unique_ptr<Observation>>
      get_file_observation_future;
  watcher_manager().GetFileObservation(
      file_url, get_file_observation_future.GetCallback());
  EXPECT_TRUE(get_file_observation_future.Get());

  ChangeAccumulator file_accumulator(get_file_observation_future.Take());
  EXPECT_TRUE(watcher_manager().HasObservationForTesting(
      file_accumulator.observation()));

  // Only observed by `dir_accumulator`.
  source.Signal(/*relative_path=*/base::FilePath(), /*error=*/false);
  // Observed by both accumulators.
  source.Signal(/*relative_path=*/file_path.BaseName(), /*error=*/false);

  std::list<Change> expected_dir_changes = {{dir_url, /*error=*/false},
                                            {file_url, /*error=*/false}};
  std::list<Change> expected_file_changes = {{file_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_dir_changes))(
               dir_accumulator.changes()) &&
           testing::Matches(testing::ContainerEq(expected_file_changes))(
               file_accumulator.changes());
  }));
}

TEST_F(FileSystemAccessWatcherManagerTest, ErroredChange) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  FakeChangeSource source(
      FileSystemAccessWatchScope::GetScopeForFileWatch(file_url));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  // Attempting to observe a scope covered by `source` will use `source`.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  source.Signal(/*relative_path=*/base::FilePath(), /*error=*/true);

  std::list<Change> expected_changes = {{file_url, /*error=*/true}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
}

TEST_F(FileSystemAccessWatcherManagerTest, ChangeAtRelativePath) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("foo");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);

  FakeChangeSource source(FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
      dir_url, /*is_recursive=*/true));
  watcher_manager().RegisterSource(&source);
  EXPECT_TRUE(watcher_manager().HasSourceForTesting(&source));

  // Attempting to observe a scope covered by `source` will use `source`.
  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetDirectoryObservation(
      dir_url, /*is_recursive=*/true, get_observation_future.GetCallback());
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  auto relative_path =
      base::FilePath::FromASCII("nested").AppendASCII("subdir");
  source.Signal(relative_path, /*error=*/false);

  std::list<Change> expected_changes = {
      {manager_->CreateFileSystemURLFromPath(
           FileSystemAccessEntryFactory::PathType::kLocal,
           dir_path.Append(relative_path)),
       /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
}

// TODO(https://crbug.com/1019297): Consider parameterizing these tests once
// observing changes to other backends is supported.

TEST_F(FileSystemAccessWatcherManagerTest, WatchLocalDirectory) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);

  base::CreateDirectory(dir_path);
  auto file_path = dir_path.AppendASCII("foo");
  base::WriteFile(file_path, "watch me");

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetDirectoryObservation(
      dir_url,
      /*is_recursive=*/false, get_observation_future.GetCallback());
// Watching the local file system is not supported on Android or Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(get_observation_future.Get());
#else
  ASSERT_TRUE(get_observation_future.Get());
  // Constructing an observation registers it with the manager.
  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  // Delete a file in the directory. This should be reported to `accumulator`.
  base::DeleteFile(file_path);

  auto expected_url =
      ReportsModifiedPathForLocalObservations()
          ? manager_->CreateFileSystemURLFromPath(
                FileSystemAccessEntryFactory::PathType::kLocal, file_path)
          : dir_url;
  std::list<Change> expected_changes = {{expected_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

TEST_F(FileSystemAccessWatcherManagerTest,
       WatchLocalDirectoryNonRecursivelyDoesNotSeeRecursiveChanges) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);

  // Create a file within a subdirectory of the directory being watched.
  base::CreateDirectory(dir_path);
  base::CreateDirectory(dir_path.AppendASCII("subdir"));
  auto file_path = dir_path.AppendASCII("subdir").AppendASCII("foo");
  base::WriteFile(file_path, "watch me");

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetDirectoryObservation(
      dir_url,
      /*is_recursive=*/false, get_observation_future.GetCallback());
  // Watching the local file system is not supported on Android or Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(get_observation_future.Get());
#else
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));
  EXPECT_TRUE(watcher_manager().HasSourcesForTesting());

  // Delete a file in the sub-directory. This should _not_ be reported to
  // `accumulator`.
  base::DeleteFile(file_path);

  // No events should be received, since this change falls outside the scope
  // of this observation.
  SpinEventLoopForABit();
  EXPECT_THAT(accumulator.changes(), testing::IsEmpty());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

TEST_F(FileSystemAccessWatcherManagerTest, WatchLocalDirectoryRecursively) {
  base::FilePath dir_path = dir_.GetPath().AppendASCII("dir");
  auto dir_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, dir_path);

  // Create a file within a subdirectory of the directory being watched.
  base::CreateDirectory(dir_path);
  base::CreateDirectory(dir_path.AppendASCII("subdir"));
  auto file_path = dir_path.AppendASCII("subdir").AppendASCII("foo");
  base::WriteFile(file_path, "watch me");

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetDirectoryObservation(
      dir_url,
      /*is_recursive=*/true, get_observation_future.GetCallback());
  // Watching the local file system is not supported on Android or Fuchsia.
  // Recursive watching of the local file system is not supported on iOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
  EXPECT_FALSE(get_observation_future.Get());
#else
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));
  EXPECT_TRUE(watcher_manager().HasSourcesForTesting());

  // TODO(https://crbug.com/1432064): Ensure that no events are reported by this
  // point.

  // Delete a file in the sub-directory. This should be reported to
  // `accumulator`.
  base::DeleteFile(file_path);

  auto expected_url =
      ReportsModifiedPathForLocalObservations()
          ? manager_->CreateFileSystemURLFromPath(
                FileSystemAccessEntryFactory::PathType::kLocal, file_path)
          : dir_url;
  std::list<Change> expected_changes = {{expected_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::Not(testing::IsEmpty()))(
        accumulator.changes());
  }));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
}

TEST_F(FileSystemAccessWatcherManagerTest, WatchLocalFile) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  // Create the file to be watched.
  base::WriteFile(file_path, "watch me");

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  // Watching the local file system is not supported on Android or Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(get_observation_future.Get());
#else
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  // Deleting the watched file should notify `accumulator`.
  base::DeleteFile(file_path);

  std::list<Change> expected_changes = {{file_url, /*error=*/false}};
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(testing::ContainerEq(expected_changes))(
        accumulator.changes());
  }));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

TEST_F(FileSystemAccessWatcherManagerTest,
       WatchLocalFileWithMultipleObservations) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  // Create the file to be watched.
  base::WriteFile(file_path, "watch me");

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future1,
      get_observation_future2, get_observation_future3;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future1.GetCallback());
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future2.GetCallback());
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future3.GetCallback());
  // Watching the local file system is not supported on Android or Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(get_observation_future1.Get());
  EXPECT_FALSE(get_observation_future2.Get());
  EXPECT_FALSE(get_observation_future3.Get());
#else
  ASSERT_TRUE(get_observation_future1.Get());
  ASSERT_TRUE(get_observation_future2.Get());
  ASSERT_TRUE(get_observation_future3.Get());

  ChangeAccumulator accumulator1(get_observation_future1.Take());
  ChangeAccumulator accumulator2(get_observation_future2.Take());
  ChangeAccumulator accumulator3(get_observation_future3.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator1.observation()));
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator2.observation()));
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator3.observation()));

  // Deleting the watched file should notify each `accumulator`.
  base::DeleteFile(file_path);

  std::list<Change> expected_changes = {{file_url, /*error=*/false}};
  const auto expected_changes_matcher = testing::ContainerEq(expected_changes);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return testing::Matches(expected_changes_matcher)(accumulator1.changes()) &&
           testing::Matches(expected_changes_matcher)(accumulator2.changes()) &&
           testing::Matches(expected_changes_matcher)(accumulator3.changes());
  }));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

TEST_F(FileSystemAccessWatcherManagerTest, OutOfScope) {
  base::FilePath file_path = dir_.GetPath().AppendASCII("foo");
  auto file_url = manager_->CreateFileSystemURLFromPath(
      FileSystemAccessEntryFactory::PathType::kLocal, file_path);

  base::test::TestFuture<std::unique_ptr<Observation>> get_observation_future;
  watcher_manager().GetFileObservation(file_url,
                                       get_observation_future.GetCallback());
  // Watching the local file system is not supported on Android or Fuchsia.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_FALSE(get_observation_future.Get());
#else
  ASSERT_TRUE(get_observation_future.Get());

  ChangeAccumulator accumulator(get_observation_future.Take());
  EXPECT_TRUE(
      watcher_manager().HasObservationForTesting(accumulator.observation()));

  // Making a change to a sibling of the watched file should _not_ report a
  // change to the accumulator.
  base::FilePath sibling_path = file_path.DirName().AppendASCII("sibling");
  base::WriteFile(sibling_path, "do not watch me");

  // Give unexpected events a chance to arrive.
  SpinEventLoopForABit();

  EXPECT_THAT(accumulator.changes(), testing::IsEmpty());
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace content
