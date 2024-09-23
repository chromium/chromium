// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FileSystemDispatcher::WriteListener
    : public mojom::blink::FileSystemOperationListener {
 public:
  WriteListener(const WriteCallback& success_callback,
                StatusCallback error_callback)
      : error_callback_(std::move(error_callback)),
        write_callback_(success_callback) {}

  void ResultsRetrieved(
      Vector<filesystem::mojom::blink::DirectoryEntryPtr> entries,
      bool has_more) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ErrorOccurred(base::File::Error error_code) override {
    std::move(error_callback_).Run(error_code);
  }

  void DidWrite(int64_t byte_count, bool complete) override {
    write_callback_.Run(byte_count, complete);
  }

 private:
  StatusCallback error_callback_;
  WriteCallback write_callback_;
};

class FileSystemDispatcher::ReadDirectoryListener
    : public mojom::blink::FileSystemOperationListener {
 public:
  explicit ReadDirectoryListener(std::unique_ptr<EntriesCallbacks> callbacks)
      : callbacks_(std::move(callbacks)) {}

  void ResultsRetrieved(
      Vector<filesystem::mojom::blink::DirectoryEntryPtr> entries,
      bool has_more) override {
    for (const auto& entry : entries) {
      callbacks_->DidReadDirectoryEntry(
          FilePathToWebString(entry->name),
          entry->type == filesystem::mojom::blink::FsFileType::DIRECTORY);
    }
    callbacks_->DidReadDirectoryEntries(has_more);
  }

  void ErrorOccurred(base::File::Error error_code) override {
    callbacks_->DidFail(error_code);
  }

  void DidWrite(int64_t byte_count, bool complete) override {
    NOTREACHED_IN_MIGRATION();
  }

 private:
  std::unique_ptr<EntriesCallbacks> callbacks_;
};

FileSystemDispatcher::FileSystemDispatcher(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      file_system_manager_(&context),
      next_operation_id_(1),
      op_listeners_(&context) {}

// static
const char FileSystemDispatcher::kSupplementName[] = "FileSystemDispatcher";

// static
FileSystemDispatcher& FileSystemDispatcher::From(ExecutionContext* context) {
  DCHECK(context);
  FileSystemDispatcher* dispatcher =
      Supplement<ExecutionContext>::From<FileSystemDispatcher>(context);
  if (!dispatcher) {
    dispatcher = MakeGarbageCollected<FileSystemDispatcher>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, dispatcher);
  }
  return *dispatcher;
}

FileSystemDispatcher::~FileSystemDispatcher() = default;

mojom::blink::FileSystemManager& FileSystemDispatcher::GetFileSystemManager() {
  if (!file_system_manager_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types
    mojo::PendingReceiver<mojom::blink::FileSystemManager> receiver =
        file_system_manager_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(
                blink::TaskType::kMiscPlatformAPI));

    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        std::move(receiver));
  }
  DCHECK(file_system_manager_.is_bound());
  return *file_system_manager_.get();
}

void FileSystemDispatcher::OpenFileSystem(
    const SecurityOrigin* origin,
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks) {
  GetFileSystemManager().Open(
      origin, type,
      WTF::BindOnce(&FileSystemDispatcher::DidOpenFileSystem,
                    WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::OpenFileSystemSync(
    const SecurityOrigin* origin,
    mojom::blink::FileSystemType type,
    std::unique_ptr<FileSystemCallbacks> callbacks) {
  String name;
  KURL root_url;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Open(origin, type, &name, &root_url, &error_code);
  DidOpenFileSystem(std::move(callbacks), std::move(name), root_url,
                    error_code);
}

void FileSystemDispatcher::ResolveURL(
    const KURL& filesystem_url,
    std::unique_ptr<ResolveURICallbacks> callbacks) {
  GetFileSystemManager().ResolveURL(
      filesystem_url,
      WTF::BindOnce(&FileSystemDispatcher::DidResolveURL,
                    WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::ResolveURLSync(
    const KURL& filesystem_url,
    std::unique_ptr<ResolveURICallbacks> callbacks) {
  mojom::blink::FileSystemInfoPtr info;
  base::FilePath file_path;
  bool is_directory;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ResolveURL(filesystem_url, &info, &file_path,
                                    &is_directory, &error_code);
  DidResolveURL(std::move(callbacks), std::move(info), std::move(file_path),
                is_directory, error_code);
}

void FileSystemDispatcher::Move(const KURL& src_path,
                                const KURL& dest_path,
                                std::unique_ptr<EntryCallbacks> callbacks) {
  GetFileSystemManager().Move(
      src_path, dest_path,
      WTF::BindOnce(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::MoveSync(const KURL& src_path,
                                    const KURL& dest_path,
                                    std::unique_ptr<EntryCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Move(src_path, dest_path, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Copy(const KURL& src_path,
                                const KURL& dest_path,
                                std::unique_ptr<EntryCallbacks> callbacks) {
  GetFileSystemManager().Copy(
      src_path, dest_path,
      WTF::BindOnce(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::CopySync(const KURL& src_path,
                                    const KURL& dest_path,
                                    std::unique_ptr<EntryCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Copy(src_path, dest_path, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Remove(const KURL& path,
                                  bool recursive,
                                  std::unique_ptr<VoidCallbacks> callbacks) {
  GetFileSystemManager().Remove(
      path, recursive,
      WTF::BindOnce(&FileSystemDispatcher::DidRemove, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::RemoveSync(
    const KURL& path,
    bool recursive,
    std::unique_ptr<VoidCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Remove(path, recursive, &error_code);
  DidRemove(std::move(callbacks), error_code);
}

void FileSystemDispatcher::ReadMetadata(
    const KURL& path,
    std::unique_ptr<MetadataCallbacks> callbacks) {
  GetFileSystemManager().ReadMetadata(
      path, WTF::BindOnce(&FileSystemDispatcher::DidReadMetadata,
                          WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::ReadMetadataSync(
    const KURL& path,
    std::unique_ptr<MetadataCallbacks> callbacks) {
  base::File::Info file_info;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadMetadata(path, &file_info, &error_code);
  DidReadMetadata(std::move(callbacks), std::move(file_info), error_code);
}

void FileSystemDispatcher::CreateFile(
    const KURL& path,
    bool exclusive,
    std::unique_ptr<EntryCallbacks> callbacks) {
  GetFileSystemManager().Create(
      path, exclusive, /*is_directory=*/false, /*is_recursive=*/false,
      WTF::BindOnce(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::CreateFileSync(
    const KURL& path,
    bool exclusive,
    std::unique_ptr<EntryCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, /*is_directory=*/false,
                                /*is_recursive=*/false, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::CreateDirectory(
    const KURL& path,
    bool exclusive,
    bool recursive,
    std::unique_ptr<EntryCallbacks> callbacks) {
  GetFileSystemManager().Create(
      path, exclusive, /*is_directory=*/true, recursive,
      WTF::BindOnce(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::CreateDirectorySync(
    const KURL& path,
    bool exclusive,
    bool recursive,
    std::unique_ptr<EntryCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, /*is_directory=*/true,
                                recursive, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Exists(const KURL& path,
                                  bool is_directory,
                                  std::unique_ptr<EntryCallbacks> callbacks) {
  GetFileSystemManager().Exists(
      path, is_directory,
      WTF::BindOnce(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                    std::move(callbacks)));
}

void FileSystemDispatcher::ExistsSync(
    const KURL& path,
    bool is_directory,
    std::unique_ptr<EntryCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Exists(path, is_directory, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::ReadDirectory(
    const KURL& path,
    std::unique_ptr<EntriesCallbacks> callbacks) {
  mojo::PendingRemote<mojom::blink::FileSystemOperationListener> listener;
  mojo::PendingReceiver<mojom::blink::FileSystemOperationListener> receiver =
      listener.InitWithNewPipeAndPassReceiver();
  op_listeners_.Add(
      std::make_unique<ReadDirectoryListener>(std::move(callbacks)),
      std::move(receiver),
      // See https://bit.ly/2S0zRAS for task types
      GetSupplementable()->GetTaskRunner(blink::TaskType::kMiscPlatformAPI));
  GetFileSystemManager().ReadDirectory(path, std::move(listener));
}

void FileSystemDispatcher::ReadDirectorySync(
    const KURL& path,
    std::unique_ptr<EntriesCallbacks> callbacks) {
  Vector<filesystem::mojom::blink::DirectoryEntryPtr> entries;
  base::File::Error result = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadDirectorySync(path, &entries, &result);
  if (result == base::File::FILE_OK) {
    DidReadDirectory(std::move(callbacks), std::move(entries),
                     std::move(result));
  }
}

void FileSystemDispatcher::InitializeFileWriter(
    const KURL& path,
    std::unique_ptr<FileWriterCallbacks> callbacks) {
  GetFileSystemManager().ReadMetadata(
      path,
      WTF::BindOnce(&FileSystemDispatcher::InitializeFileWriterCallback,
                    WrapWeakPersistent(this), path, std::move(callbacks)));
}

void FileSystemDispatcher::InitializeFileWriterSync(
    const KURL& path,
    std::unique_ptr<FileWriterCallbacks> callbacks) {
  base::File::Info file_info;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadMetadata(path, &file_info, &error_code);
  InitializeFileWriterCallback(path, std::move(callbacks), file_info,
                               error_code);
}

void FileSystemDispatcher::Truncate(const KURL& path,
                                    int64_t offset,
                                    int* request_id_out,
                                    StatusCallback callback) {
  HeapMojoRemote<mojom::blink::FileSystemCancellableOperation> op_remote(
      GetSupplementable());
  // See https://bit.ly/2S0zRAS for task types
  mojo::PendingReceiver<mojom::blink::FileSystemCancellableOperation>
      op_receiver = op_remote.BindNewPipeAndPassReceiver(
          GetSupplementable()->GetTaskRunner(
              blink::TaskType::kMiscPlatformAPI));
  int operation_id = next_operation_id_++;
  op_remote.set_disconnect_handler(
      WTF::BindOnce(&FileSystemDispatcher::RemoveOperationRemote,
                    WrapWeakPersistent(this), operation_id));
  cancellable_operations_.insert(operation_id,
                                 WrapDisallowNew(std::move(op_remote)));
  GetFileSystemManager().Truncate(
      path, offset, std::move(op_receiver),
      WTF::BindOnce(&FileSystemDispatcher::DidTruncate,
                    WrapWeakPersistent(this), operation_id,
                    std::move(callback)));

  if (request_id_out)
    *request_id_out = operation_id;
}

void FileSystemDispatcher::TruncateSync(const KURL& path,
                                        int64_t offset,
                                        StatusCallback callback) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().TruncateSync(path, offset, &error_code);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::Write(const KURL& path,
                                 const Blob& blob,
                                 int64_t offset,
                                 int* request_id_out,
                                 const WriteCallback& success_callback,
                                 StatusCallback error_callback) {
  HeapMojoRemote<mojom::blink::FileSystemCancellableOperation> op_remote(
      GetSupplementable());
  // See https://bit.ly/2S0zRAS for task types
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      GetSupplementable()->GetTaskRunner(blink::TaskType::kMiscPlatformAPI);
  mojo::PendingReceiver<mojom::blink::FileSystemCancellableOperation>
      op_receiver = op_remote.BindNewPipeAndPassReceiver(task_runner);
  int operation_id = next_operation_id_++;
  op_remote.set_disconnect_handler(
      WTF::BindOnce(&FileSystemDispatcher::RemoveOperationRemote,
                    WrapWeakPersistent(this), operation_id));
  cancellable_operations_.insert(operation_id,
                                 WrapDisallowNew(std::move(op_remote)));

  mojo::PendingRemote<mojom::blink::FileSystemOperationListener> listener;
  mojo::PendingReceiver<mojom::blink::FileSystemOperationListener> receiver =
      listener.InitWithNewPipeAndPassReceiver();
  op_listeners_.Add(std::make_unique<WriteListener>(
                        WTF::BindRepeating(&FileSystemDispatcher::DidWrite,
                                           WrapWeakPersistent(this),
                                           success_callback, operation_id),
                        WTF::BindOnce(&FileSystemDispatcher::WriteErrorCallback,
                                      WrapWeakPersistent(this),
                                      std::move(error_callback), operation_id)),
                    std::move(receiver), task_runner);

  GetFileSystemManager().Write(path, blob.AsMojoBlob(), offset,
                               std::move(op_receiver), std::move(listener));

  if (request_id_out)
    *request_id_out = operation_id;
}

void FileSystemDispatcher::WriteSync(const KURL& path,
                                     const Blob& blob,
                                     int64_t offset,
                                     const WriteCallback& success_callback,
                                     StatusCallback error_callback) {
  int64_t byte_count;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().WriteSync(path, blob.AsMojoBlob(), offset, &byte_count,
                                   &error_code);
  if (error_code == base::File::FILE_OK)
    std::move(success_callback).Run(byte_count, /*complete=*/true);
  else
    std::move(error_callback).Run(error_code);
}

void FileSystemDispatcher::Cancel(int request_id_to_cancel,
                                  StatusCallback callback) {
  if (!base::Contains(cancellable_operations_, request_id_to_cancel)) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  auto& remote =
      cancellable_operations_.find(request_id_to_cancel)->value->Value();
  if (!remote.is_bound()) {
    RemoveOperationRemote(request_id_to_cancel);
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  remote->Cancel(WTF::BindOnce(&FileSystemDispatcher::DidCancel,
                               WrapWeakPersistent(this), std::move(callback),
                               request_id_to_cancel));
}

void FileSystemDispatcher::CreateSnapshotFile(
    const KURL& file_path,
    std::unique_ptr<SnapshotFileCallbackBase> callbacks) {
  GetFileSystemManager().CreateSnapshotFile(
      file_path, WTF::BindOnce(&FileSystemDispatcher::DidCreateSnapshotFile,
                               WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::CreateSnapshotFileSync(
    const KURL& file_path,
    std::unique_ptr<SnapshotFileCallbackBase> callbacks) {
  base::File::Info file_info;
  base::FilePath platform_path;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  mojo::PendingRemote<mojom::blink::ReceivedSnapshotListener> listener;
  GetFileSystemManager().CreateSnapshotFile(
      file_path, &file_info, &platform_path, &error_code, &listener);
  DidCreateSnapshotFile(std::move(callbacks), std::move(file_info),
                        std::move(platform_path), error_code,
                        std::move(listener));
}

void FileSystemDispatcher::Trace(Visitor* visitor) const {
  visitor->Trace(file_system_manager_);
  visitor->Trace(cancellable_operations_);
  visitor->Trace(op_listeners_);
  Supplement<ExecutionContext>::Trace(visitor);
}

void FileSystemDispatcher::DidOpenFileSystem(
    std::unique_ptr<FileSystemCallbacks> callbacks,
    const String& name,
    const KURL& root,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    callbacks->DidOpenFileSystem(name, root);
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::DidResolveURL(
    std::unique_ptr<ResolveURICallbacks> callbacks,
    mojom::blink::FileSystemInfoPtr info,
    const base::FilePath& file_path,
    bool is_directory,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    DCHECK(info->root_url.IsValid());
    callbacks->DidResolveURL(info->name, info->root_url, info->mount_type,
                             FilePathToWebString(file_path), is_directory);
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::DidRemove(std::unique_ptr<VoidCallbacks> callbacks,
                                     base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK)
    callbacks->DidSucceed();
  else
    callbacks->DidFail(error_code);
}

void FileSystemDispatcher::DidFinish(std::unique_ptr<EntryCallbacks> callbacks,
                                     base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK)
    callbacks->DidSucceed();
  else
    callbacks->DidFail(error_code);
}

void FileSystemDispatcher::DidReadMetadata(
    std::unique_ptr<MetadataCallbacks> callbacks,
    const base::File::Info& file_info,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    callbacks->DidReadMetadata(FileMetadata::From(file_info));
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::DidReadDirectory(
    std::unique_ptr<EntriesCallbacks> callbacks,
    Vector<filesystem::mojom::blink::DirectoryEntryPtr> entries,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    for (const auto& entry : entries) {
      callbacks->DidReadDirectoryEntry(
          FilePathToWebString(entry->name),
          entry->type == filesystem::mojom::blink::FsFileType::DIRECTORY);
    }
    callbacks->DidReadDirectoryEntries(false);
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::InitializeFileWriterCallback(
    const KURL& path,
    std::unique_ptr<FileWriterCallbacks> callbacks,
    const base::File::Info& file_info,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    if (file_info.is_directory || file_info.size < 0) {
      callbacks->DidFail(base::File::FILE_ERROR_FAILED);
      return;
    }
    callbacks->DidCreateFileWriter(path, file_info.size);
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::DidTruncate(int operation_id,
                                       StatusCallback callback,
                                       base::File::Error error_code) {
  if (error_code != base::File::FILE_ERROR_ABORT)
    RemoveOperationRemote(operation_id);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::DidWrite(const WriteCallback& callback,
                                    int operation_id,
                                    int64_t bytes,
                                    bool complete) {
  callback.Run(bytes, complete);
  if (complete)
    RemoveOperationRemote(operation_id);
}

void FileSystemDispatcher::WriteErrorCallback(StatusCallback callback,
                                              int operation_id,
                                              base::File::Error error) {
  std::move(callback).Run(error);
  if (error != base::File::FILE_ERROR_ABORT)
    RemoveOperationRemote(operation_id);
}

void FileSystemDispatcher::DidCancel(StatusCallback callback,
                                     int cancelled_operation_id,
                                     base::File::Error error_code) {
  if (error_code == base::File::FILE_OK)
    RemoveOperationRemote(cancelled_operation_id);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::DidCreateSnapshotFile(
    std::unique_ptr<SnapshotFileCallbackBase> callbacks,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    base::File::Error error_code,
    mojo::PendingRemote<mojom::blink::ReceivedSnapshotListener> listener) {
  if (error_code == base::File::FILE_OK) {
    FileMetadata file_metadata = FileMetadata::From(file_info);
    file_metadata.platform_path = FilePathToWebString(platform_path);

    callbacks->DidCreateSnapshotFile(file_metadata);

    if (listener) {
      mojo::Remote<mojom::blink::ReceivedSnapshotListener>(std::move(listener))
          ->DidReceiveSnapshotFile();
    }
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::RemoveOperationRemote(int operation_id) {
  auto it = cancellable_operations_.find(operation_id);
  if (it == cancellable_operations_.end())
    return;
  cancellable_operations_.erase(it);
}

}  // namespace blink
