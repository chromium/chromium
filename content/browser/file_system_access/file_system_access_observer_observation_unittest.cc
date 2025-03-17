// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <memory>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"

namespace content {

using ChangeInfo = FilePathWatcher::ChangeInfo;
using ChangeType = FilePathWatcher::ChangeType;
using FilePathType = FilePathWatcher::FilePathType;

using MojoChangeType = blink::mojom::internal::FileSystemAccessChangeType_Data::
    FileSystemAccessChangeType_Tag;
using MojoFilePathType = blink::mojom::internal::FileSystemAccessHandle_Data::
    FileSystemAccessHandle_Tag;

namespace {

// A struct equivalent to `blink::mojom::FileSystemAccessChangePtr` but can be
// used in gtest matchers.
struct MojoChangeInfo {
  static MojoChangeInfo FromMojo(
      const blink::mojom::FileSystemAccessChangePtr& mojo_change) {
    std::optional<std::vector<std::string>> relative_moved_from_path;
    if (mojo_change->type->is_moved()) {
      relative_moved_from_path =
          mojo_change->type->get_moved()->former_relative_path;
    }
    return {mojo_change->type->which(),
            mojo_change->metadata->changed_entry->entry_handle->which(),
            mojo_change->metadata->relative_path, relative_moved_from_path};
  }

  MojoChangeType change_type;
  MojoFilePathType file_path_type;

  std::vector<std::string> relative_path;
  std::optional<std::vector<std::string>> relative_moved_from_path;

  friend bool operator==(MojoChangeInfo rhs, MojoChangeInfo lhs) {
    return rhs.change_type == lhs.change_type &&
           rhs.file_path_type == lhs.file_path_type &&
           rhs.relative_path == lhs.relative_path &&
           rhs.relative_moved_from_path == lhs.relative_moved_from_path;
  }
};

// Implementation of an observation (blink::mojom::FileSystemAccessObserver).
// Collects the changes it receives so that it can be verified against expected
// changes.
class FakeObservation : public blink::mojom::FileSystemAccessObserver {
 public:
  explicit FakeObservation(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessObserver>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeObservation::OnRemoteDisconnected, weak_factory_.GetWeakPtr()));
  }

  // Returns true if the the events received since the last call to this
  // function matches the `expected_changes`.
  bool EventsReceivedMatches(std::vector<MojoChangeInfo> expected_changes) {
    return testing::Matches(testing::ContainerEq(CollectEvents()))(
        expected_changes);
  }

  bool RemoteObservationDisconnected() {
    return remote_observation_disconnected_;
  }

 private:
  // blink::mojom::FileSystemAccessObserver
  void OnFileChanges(std::vector<blink::mojom::FileSystemAccessChangePtr>
                         mojo_changes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::transform(mojo_changes.begin(), mojo_changes.end(),
                   std::back_inserter(collected_events_),
                   MojoChangeInfo::FromMojo);
  }

  using CollectEventsCallback =
      base::OnceCallback<void(std::vector<MojoChangeInfo>)>;

  void CollectEventsImpl(CollectEventsCallback future_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto finish_collecting_events_callback = base::BindOnce(
        &FakeObservation::FinishCollectingEvents, weak_factory_.GetWeakPtr(),
        collected_events_.size(), std::move(future_callback));

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(finish_collecting_events_callback),
        TestTimeouts::tiny_timeout());
  }

  void FinishCollectingEvents(size_t collected_events_previous_size,
                              CollectEventsCallback future_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (collected_events_.size() == collected_events_previous_size) {
      std::move(future_callback).Run(std::move(collected_events_));
    } else {
      CollectEventsImpl(std::move(future_callback));
    }
  }

  std::vector<MojoChangeInfo> CollectEvents() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::test::TestFuture<std::vector<MojoChangeInfo>> future;

    CollectEventsImpl(future.GetCallback());

    return future.Take();
  }

  void OnRemoteDisconnected() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    remote_observation_disconnected_ = true;
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<MojoChangeInfo> collected_events_;

  mojo::Receiver<blink::mojom::FileSystemAccessObserver> receiver_;

  bool remote_observation_disconnected_ = false;

  base::WeakPtrFactory<FakeObservation> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// Wraps an observer remote
// (mojo::Remote<blink::mojom::FileSystemAccessObserverHost>).
class FakeObserver {
 public:
  explicit FakeObserver(
      mojo::Remote<blink::mojom::FileSystemAccessObserverHost> observer)
      : observer_(std::move(observer)) {}

  FakeObservation Observe(std::variant<FileSystemAccessFileHandleImpl*,
                                       FileSystemAccessDirectoryHandleImpl*>
                              file_or_directory_handle,
                          bool recursive) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    CHECK(observer_.is_bound());

    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        transfer_token;

    std::visit(
        [&transfer_token](auto* handle) {
          handle->Transfer(transfer_token.InitWithNewPipeAndPassReceiver());
        },
        file_or_directory_handle);

    base::test::TestFuture<
        blink::mojom::FileSystemAccessErrorPtr,
        mojo::PendingReceiver<blink::mojom::FileSystemAccessObserver>>
        future;

    observer_->Observe(std::move(transfer_token), recursive,
                       future.GetCallback());

    auto [result, observer_receiver] = future.Take();

    CHECK(result->status == blink::mojom::FileSystemAccessStatus::kOk);

    return FakeObservation(std::move(observer_receiver));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<blink::mojom::FileSystemAccessObserverHost> observer_;
};

// Trivial implementation of a change source which allows tests to signal
// changes.
class FakeChangeSource : public FileSystemAccessChangeSource {
 public:
  explicit FakeChangeSource(
      FileSystemAccessWatchScope scope,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      FileSystemAccessWatcherManager& watcher_manager)
      : FileSystemAccessChangeSource(std::move(scope),
                                     std::move(file_system_context)) {
    watcher_manager.RegisterSourceForTesting(this);
    EXPECT_TRUE(watcher_manager.HasSourceForTesting(this));
  }
  FakeChangeSource(const FakeChangeSource&) = delete;
  FakeChangeSource& operator=(const FakeChangeSource&) = delete;
  ~FakeChangeSource() override = default;

  // FileSystemAccessChangeSource:
  void Initialize(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_initialized) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(on_initialized).Run(initialization_result_->Clone());
  }

  void SignalChange(ChangeInfo change_info = ChangeInfo()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::FilePath relative_path;
    scope().root_url().virtual_path().AppendRelativePath(
        change_info.modified_path, &relative_path);
    NotifyOfChange(std::move(relative_path), false, std::move(change_info));
  }

  void SignalUsageChange(size_t old_usage, size_t new_usage) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    NotifyOfUsageChange(old_usage, new_usage);
  }

  void SignalError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NotifyOfChange(base::FilePath(), true, ChangeInfo());
  }

 private:
  blink::mojom::FileSystemAccessErrorPtr initialization_result_
      GUARDED_BY_CONTEXT(sequence_checker_) =
          blink::mojom::FileSystemAccessError::New(
              blink::mojom::FileSystemAccessStatus::kOk,
              base::File::FILE_OK,
              "");
};

}  // namespace

class FileSystemAccessObserverObservationTest
    : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    RenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, temp_dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<FileSystemAccessManagerImpl>(
        file_system_context_, chrome_blob_context_,
        /*permission_context=*/nullptr,
        /*off_the_record=*/false);
  }

  void TearDown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    manager_.reset();

    task_environment()->RunUntilIdle();
    EXPECT_TRUE(temp_dir_.Delete());

    chrome_blob_context_.reset();

    RenderViewHostTestHarness::TearDown();
  }

  scoped_refptr<storage::FileSystemContext>& file_system_context() {
    return file_system_context_;
  }

  void RegisterChangeSource(FakeChangeSource& source) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    manager_->watcher_manager().RegisterSourceForTesting(&source);
    EXPECT_TRUE(manager_->watcher_manager().HasSourceForTesting(&source));
  }

  void EnterBFCache() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
    rfh->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  }

  void ExitBFCache() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
    rfh->SetLifecycleState(RenderFrameHostImpl::LifecycleStateImpl::kActive);
  }

  FakeChangeSource CreateFileChangeSource(storage::FileSystemURL file_url) {
    return FakeChangeSource(
        FileSystemAccessWatchScope::GetScopeForFileWatch(file_url),
        file_system_context_, manager_->watcher_manager());
  }

  base::FilePath CreateFile() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath file_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(
          base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
      EXPECT_TRUE(base::WriteFile(file_path, "observe me"));
    }
    return file_path;
  }

  storage::FileSystemURL CreateFileSystemURL(const base::FilePath& file_path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return manager_->CreateFileSystemURLFromPath(PathInfo(file_path));
  }

  std::unique_ptr<FileSystemAccessFileHandleImpl> CreateFileHandle(
      const storage::FileSystemURL& file_url,
      const base::FilePath& display_name) {
    return CreateFileHandle(kTestOrigin, file_url, display_name);
  }

  std::unique_ptr<FileSystemAccessFileHandleImpl> CreateFileHandle(
      const std::string& origin,
      const storage::FileSystemURL& file_url,
      const base::FilePath& display_name) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return std::make_unique<FileSystemAccessFileHandleImpl>(
        manager_.get(), GetBindingContext(origin), file_url,
        display_name.AsUTF8Unsafe(),
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
  }

  FakeChangeSource CreateDirectoryChangeSource(storage::FileSystemURL dir_url,
                                               bool is_recursive) {
    return FakeChangeSource(
        FileSystemAccessWatchScope::GetScopeForDirectoryWatch(dir_url,
                                                              is_recursive),
        file_system_context_, manager_->watcher_manager());
  }

  base::FilePath CreateDirectory() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath dir_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::CreateTemporaryDirInDir(
          temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &dir_path));
    }
    return dir_path;
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> CreateDirectoryHandle(
      const storage::FileSystemURL& dir_url) {
    return CreateDirectoryHandle(kTestOrigin, dir_url);
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> CreateDirectoryHandle(
      const std::string& origin,
      const storage::FileSystemURL& dir_url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return std::make_unique<FileSystemAccessDirectoryHandleImpl>(
        manager_.get(), GetBindingContext(origin), dir_url,
        FileSystemAccessManagerImpl::SharedHandleState(allow_grant_,
                                                       allow_grant_));
  }

  FakeObserver CreateObserver() { return CreateObserver(kTestOrigin); }

  FakeObserver CreateObserver(const std::string& origin) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mojo::Remote<blink::mojom::FileSystemAccessObserverHost> observer;

    mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
        host_receiver = observer.BindNewPipeAndPassReceiver();

    FakeObserver fake_observer(std::move(observer));

    manager_->watcher_manager().BindObserverHost(GetBindingContext(origin),
                                                 std::move(host_receiver));
    return fake_observer;
  }

 private:
  FileSystemAccessManagerImpl::BindingContext GetBindingContext(
      const std::string& origin) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GURL url = GURL(origin);
    blink::StorageKey storage_key =
        blink::StorageKey::CreateFromStringForTesting(origin);

    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(main_rfh());

    return FileSystemAccessManagerImpl::BindingContext(storage_key, url,
                                                       rfh->GetGlobalId());
  }

  SEQUENCE_CHECKER(sequence_checker_);

  std::string kTestOrigin = "https://example.com/test";

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;
  scoped_refptr<FileSystemAccessManagerImpl> manager_;

  scoped_refptr<FixedFileSystemAccessPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          FixedFileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
          PathInfo());
};

TEST_F(FileSystemAccessObserverObservationTest,
       NoChangesAfterAnErrorIsReported) {
  base::FilePath file_path = CreateFile();
  storage::FileSystemURL file_url = CreateFileSystemURL(file_path);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle =
      CreateFileHandle(file_url, file_path.BaseName());

  FakeChangeSource source = CreateFileChangeSource(file_url);

  FakeObserver observer = CreateObserver();
  FakeObservation observation = observer.Observe(file_handle.get(), false);

  // Will receive changes before the error.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {}}}));

  // Will receive an error event after signalling an error.
  source.SignalError();
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kErrored, MojoFilePathType::kFile, {}}}));

  // Won't receive any further error events or changes.
  source.SignalError();
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches({}));
  EXPECT_TRUE(observation.RemoteObservationDisconnected());
}

TEST_F(FileSystemAccessObserverObservationTest,
       AnErrorDestroysTheCorrectObservation) {
  base::FilePath file_path1 = CreateFile();
  storage::FileSystemURL file_url1 = CreateFileSystemURL(file_path1);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle1 =
      CreateFileHandle(file_url1, file_path1.BaseName());

  FakeChangeSource source1 = CreateFileChangeSource(file_url1);

  base::FilePath file_path2 = CreateFile();
  storage::FileSystemURL file_url2 = CreateFileSystemURL(file_path2);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle2 =
      CreateFileHandle(file_url2, file_path2.BaseName());

  FakeChangeSource source2 = CreateFileChangeSource(file_url2);

  FakeObserver observer = CreateObserver();
  FakeObservation observation1 = observer.Observe(file_handle1.get(), false);
  FakeObservation observation2 = observer.Observe(file_handle2.get(), false);

  // Only `observation1` receives the error for `source`.
  source1.SignalError();
  EXPECT_TRUE(observation1.EventsReceivedMatches(
      {{MojoChangeType::kErrored, MojoFilePathType::kFile, {}}}));
  EXPECT_TRUE(observation2.EventsReceivedMatches({}));
  EXPECT_TRUE(observation1.RemoteObservationDisconnected());

  // `observation1` won't receive any further error events or changes, and
  // `observation2` will continue not to receive changes for `source1`.
  source1.SignalError();
  source1.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path1));
  EXPECT_TRUE(observation1.EventsReceivedMatches({}));
  EXPECT_TRUE(observation1.RemoteObservationDisconnected());
  EXPECT_TRUE(observation2.EventsReceivedMatches({}));

  // `observation2` continues to receive changes.
  source2.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path2));
  EXPECT_TRUE(observation1.EventsReceivedMatches({}));
  EXPECT_TRUE(observation1.RemoteObservationDisconnected());
  EXPECT_TRUE(observation2.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {}}}));
}

TEST_F(FileSystemAccessObserverObservationTest,
       FileObservationDestructedAfterFileDeleted) {
  base::FilePath file_path = CreateFile();
  storage::FileSystemURL file_url = CreateFileSystemURL(file_path);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle =
      CreateFileHandle(file_url, file_path.BaseName());

  FakeChangeSource source = CreateFileChangeSource(file_url);

  FakeObserver observer = CreateObserver();
  FakeObservation observation =
      observer.Observe(file_handle.get(), /*recursive=*/false);

  // Deleting the file should result in a disappeared and an errored event.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kDeleted, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {}},
       {MojoChangeType::kDisappeared, MojoFilePathType::kFile, {}},
       {MojoChangeType::kErrored, MojoFilePathType::kFile, {}}}));

  // The remote observation should have been destructed and disconnected. So,
  // further events should not be received.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kModified, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches({}));
  EXPECT_TRUE(observation.RemoteObservationDisconnected());
}

TEST_F(FileSystemAccessObserverObservationTest,
       FileObservationDestructedWhenParentDirectoryDeleted) {
  base::FilePath dir_path = CreateDirectory();
  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(dir_path, &file_path));
    EXPECT_TRUE(base::WriteFile(file_path, "observe me"));
  }
  storage::FileSystemURL file_url = CreateFileSystemURL(file_path);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle =
      CreateFileHandle(file_url, file_path.BaseName());

  FakeChangeSource source = CreateFileChangeSource(file_url);

  FakeObserver observer = CreateObserver();
  FakeObservation observation =
      observer.Observe(file_handle.get(), /*recursive=*/false);

  // Deleting the parent directory should result in a disappeared and an errored
  // event on the file observation.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kModified, file_path));
  // We handle directory deletion by signaling a deleted event on the file
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kDeleted, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kModified, MojoFilePathType::kFile, {}},
       {MojoChangeType::kDisappeared, MojoFilePathType::kFile, {}},
       {MojoChangeType::kErrored, MojoFilePathType::kFile, {}}}));

  // The remote observation should have been destructed and disconnected. So,
  // further events should not be received.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kModified, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches({}));
  EXPECT_TRUE(observation.RemoteObservationDisconnected());
}

// TODO(crbug.com/393229134): Reenable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DirectoryObservationDestructedAfterScopeRootDeleted \
  DISABLED_DirectoryObservationDestructedAfterScopeRootDeleted
#else
#define MAYBE_DirectoryObservationDestructedAfterScopeRootDeleted \
  DirectoryObservationDestructedAfterScopeRootDeleted
#endif
TEST_F(FileSystemAccessObserverObservationTest,
       MAYBE_DirectoryObservationDestructedAfterScopeRootDeleted) {
  base::FilePath dir_path = CreateDirectory();
  storage::FileSystemURL dir_url = CreateFileSystemURL(dir_path);
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> dir_handle =
      CreateDirectoryHandle(dir_url);

  FakeChangeSource source =
      CreateDirectoryChangeSource(dir_url, /*is_recursive=*/false);

  FakeObserver observer = CreateObserver();
  FakeObservation observation =
      observer.Observe(dir_handle.get(), /*recursive=*/false);

  base::FilePath file_path = dir_path.AppendASCII("file.txt");
  // Deleting the directory root should result in a disappeared and an errored
  // event.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  // We handle directory deletion by first signaling a deleted event and then an
  // error.
  source.SignalChange(
      ChangeInfo(FilePathType::kDirectory, ChangeType::kDeleted, dir_path));
  source.SignalError();
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {"file.txt"}},
       {MojoChangeType::kDisappeared, MojoFilePathType::kDirectory, {}},
       {MojoChangeType::kErrored, MojoFilePathType::kDirectory, {}}}));

  // The remote observation should have been destructed and disconnected. So,
  // no further events should be received.
  source.SignalChange(
      ChangeInfo(FilePathType::kDirectory, ChangeType::kModified, dir_path));
  EXPECT_TRUE(observation.EventsReceivedMatches({}));
  EXPECT_TRUE(observation.RemoteObservationDisconnected());
}

// TODO(crbug.com/393229134): Reenable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DirectoryObservationNotDestructedAfterFileDeleted \
  DISABLED_DirectoryObservationNotDestructedAfterFileDeleted
#else
#define MAYBE_DirectoryObservationNotDestructedAfterFileDeleted \
  DirectoryObservationNotDestructedAfterFileDeleted
#endif
TEST_F(FileSystemAccessObserverObservationTest,
       MAYBE_DirectoryObservationNotDestructedAfterFileDeleted) {
  base::FilePath dir_path = CreateDirectory();
  storage::FileSystemURL dir_url = CreateFileSystemURL(dir_path);
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> dir_handle =
      CreateDirectoryHandle(dir_url);

  FakeChangeSource source =
      CreateDirectoryChangeSource(dir_url, /*is_recursive=*/false);

  FakeObserver observer = CreateObserver();
  FakeObservation observation =
      observer.Observe(dir_handle.get(), /*recursive=*/false);

  base::FilePath file_path = dir_path.AppendASCII("file.txt");

  // Deleting a file in the dir should result in a disappeared but not an
  // errored event.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kDeleted, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {"file.txt"}},
       {MojoChangeType::kDisappeared, MojoFilePathType::kFile, {"file.txt"}}}));

  // The remote observation should not have been disconnected. So, we should
  // continue receiving events.
  source.SignalChange(
      ChangeInfo(FilePathType::kDirectory, ChangeType::kModified, dir_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kModified, MojoFilePathType::kDirectory, {}}}));
}

// TODO(crbug.com/393229134): Reenable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ObservationDestroyedOnQuotaExceeded \
  DISABLED_ObservationDestroyedOnQuotaExceeded
#else
#define MAYBE_ObservationDestroyedOnQuotaExceeded \
  ObservationDestroyedOnQuotaExceeded
#endif
TEST_F(FileSystemAccessObserverObservationTest,
       MAYBE_ObservationDestroyedOnQuotaExceeded) {
  base::FilePath favorites_path = CreateDirectory();
  base::FilePath documents_path = CreateDirectory();
  storage::FileSystemURL favorites_url = CreateFileSystemURL(favorites_path);
  storage::FileSystemURL documents_url = CreateFileSystemURL(documents_path);
  FakeChangeSource favorites_source =
      CreateDirectoryChangeSource(favorites_url, /*is_recursive=*/false);
  FakeChangeSource documents_source =
      CreateDirectoryChangeSource(documents_url, /*is_recursive=*/false);

  std::string foo_origin = "https://foo.com";
  std::string bar_origin = "https://bar.com";

  FakeObserver foo_observer = CreateObserver(foo_origin);
  FakeObserver bar_observer = CreateObserver(bar_origin);

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> foo_favorites_handle =
      CreateDirectoryHandle(foo_origin, favorites_url);
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> bar_favorites_handle =
      CreateDirectoryHandle(bar_origin, favorites_url);
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> foo_documents_handle =
      CreateDirectoryHandle(foo_origin, documents_url);

  FakeObservation foo_favorites_observation =
      foo_observer.Observe(foo_favorites_handle.get(), /*recursive=*/false);
  FakeObservation bar_favorites_observation =
      bar_observer.Observe(bar_favorites_handle.get(), /*recursive=*/false);
  FakeObservation foo_documents_observation =
      foo_observer.Observe(foo_documents_handle.get(), /*recursive=*/false);

  auto quota_override = FilePathWatcher::SetQuotaLimitForTesting(100);

  // Only foo is observing documents, so only its usage should rise. It is still
  // under the quota limit though.
  // - foo: (favorites usage = 0) + (documents usage = 50) = 50
  //   - 50 < 100 so no quota error
  // - bar: (favorites usage = 0) = 0
  //   - 0 < 100 so no quota error
  documents_source.SignalUsageChange(0, 50);
  EXPECT_TRUE(foo_favorites_observation.EventsReceivedMatches({}));
  EXPECT_FALSE(foo_favorites_observation.RemoteObservationDisconnected());
  EXPECT_TRUE(bar_favorites_observation.EventsReceivedMatches({}));
  EXPECT_FALSE(bar_favorites_observation.RemoteObservationDisconnected());
  EXPECT_TRUE(foo_documents_observation.EventsReceivedMatches({}));
  EXPECT_FALSE(foo_documents_observation.RemoteObservationDisconnected());

  // Both origins are observing favorites, so both usages should rise. This
  // sends foo over the quota limit but not bar. Foo's favorites observation
  // should receive an error and be disconnected.
  // - foo: (favorites usage = 60) + (documents usage = 50) = 110
  //   - 110 > 100 so quota error
  // - bar: (favorites usage = 60) = 60
  //   - 60 < 100 so no quota error
  favorites_source.SignalUsageChange(0, 60);
  EXPECT_TRUE(foo_favorites_observation.EventsReceivedMatches(
      {{MojoChangeType::kErrored, MojoFilePathType::kDirectory, {}}}));
  EXPECT_TRUE(foo_favorites_observation.RemoteObservationDisconnected());
  EXPECT_TRUE(bar_favorites_observation.EventsReceivedMatches({}));
  EXPECT_FALSE(bar_favorites_observation.RemoteObservationDisconnected());
  EXPECT_TRUE(foo_documents_observation.EventsReceivedMatches({}));
  EXPECT_FALSE(foo_documents_observation.RemoteObservationDisconnected());

  // Only bar should now receive events for favorites now.
  std::string file_name = "file.txt";
  favorites_source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated,
                 favorites_path.AppendASCII(file_name)));
  EXPECT_TRUE(foo_favorites_observation.EventsReceivedMatches({}));
  EXPECT_TRUE(bar_favorites_observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {file_name}}}));

  // Foo can still receive events for documents.
  documents_source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated,
                 documents_path.AppendASCII(file_name)));
  EXPECT_TRUE(foo_documents_observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {file_name}}}));
}

TEST_F(FileSystemAccessObserverObservationTest, ReceivedEventsInBFCache) {
  base::FilePath file_path = CreateFile();
  storage::FileSystemURL file_url = CreateFileSystemURL(file_path);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle =
      CreateFileHandle(file_url, file_path.BaseName());

  FakeChangeSource source = CreateFileChangeSource(file_url);

  FakeObserver observer = CreateObserver();
  FakeObservation observation = observer.Observe(file_handle.get(), false);

  // Will receive changes before entering BFCache.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kModified, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {}},
       {MojoChangeType::kModified, MojoFilePathType::kFile, {}}}));

  // No event is emitted just for entering the BFCache.
  EnterBFCache();
  ExitBFCache();
  EXPECT_TRUE(observation.EventsReceivedMatches({}));

  // No events are emitted while in BFCache.
  EnterBFCache();
  source.SignalChange();
  source.SignalChange();
  EXPECT_TRUE(observation.EventsReceivedMatches({}));

  // If we receive an event while in BFCache, a single unknown event is emitted
  // after exiting BFCache.
  ExitBFCache();
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kUnknown, MojoFilePathType::kFile, {}}}));
}

TEST_F(FileSystemAccessObserverObservationTest, ReceivedErrorsInBFCache) {
  base::FilePath file_path = CreateFile();
  storage::FileSystemURL file_url = CreateFileSystemURL(file_path);
  std::unique_ptr<FileSystemAccessFileHandleImpl> file_handle =
      CreateFileHandle(file_url, file_path.BaseName());

  FakeChangeSource source = CreateFileChangeSource(file_url);

  FakeObserver observer = CreateObserver();
  FakeObservation observation = observer.Observe(file_handle.get(), false);

  // Will receive changes before entering BFCache.
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kCreated, file_path));
  source.SignalChange(
      ChangeInfo(FilePathType::kFile, ChangeType::kModified, file_path));
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kAppeared, MojoFilePathType::kFile, {}},
       {MojoChangeType::kModified, MojoFilePathType::kFile, {}}}));

  // No event is emitted just for entering the BFCache.
  EnterBFCache();
  ExitBFCache();
  EXPECT_TRUE(observation.EventsReceivedMatches({}));

  // No errors are emitted while in BFCache
  EnterBFCache();
  source.SignalChange();
  source.SignalError();
  source.SignalError();
  source.SignalChange();
  EXPECT_TRUE(observation.EventsReceivedMatches({}));

  // If we receive an event while in BFCache, a single unknown event is emitted
  // after exiting BFCache.
  ExitBFCache();
  EXPECT_TRUE(observation.EventsReceivedMatches(
      {{MojoChangeType::kErrored, MojoFilePathType::kFile, {}}}));
}

}  // namespace content
