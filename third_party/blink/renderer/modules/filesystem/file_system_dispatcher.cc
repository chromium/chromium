// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/filesystem/file_system_dispatcher.h"

#include "build/build_config.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
    NOTREACHED();
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
  explicit ReadDirectoryListener(
      std::unique_ptr<AsyncFileSystemCallbacks> callbacks)
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

  void DidWrite(int64_t byte_count, bool complete) override { NOTREACHED(); }

 private:
  std::unique_ptr<AsyncFileSystemCallbacks> callbacks_;
};

FileSystemDispatcher::FileSystemDispatcher(ExecutionContext& context)
    : Supplement<ExecutionContext>(context), next_operation_id_(1) {}

// static
const char FileSystemDispatcher::kSupplementName[] = "FileSystemDispatcher";

// static
FileSystemDispatcher& FileSystemDispatcher::From(ExecutionContext* context) {
  DCHECK(context);
  FileSystemDispatcher* dispatcher =
      Supplement<ExecutionContext>::From<FileSystemDispatcher>(context);
  if (!dispatcher) {
    dispatcher = new FileSystemDispatcher(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, dispatcher);
  }
  return *dispatcher;
}

FileSystemDispatcher::~FileSystemDispatcher() = default;

mojom::blink::FileSystemManager& FileSystemDispatcher::GetFileSystemManager() {
  if (!file_system_manager_ptr_) {
    mojom::blink::FileSystemManagerRequest request =
        mojo::MakeRequest(&file_system_manager_ptr_);
    // Document::GetInterfaceProvider() can return null if the frame is
    // detached.
    if (GetSupplementable()->GetInterfaceProvider()) {
      GetSupplementable()->GetInterfaceProvider()->GetInterface(
          std::move(request));
    }
  }
  DCHECK(file_system_manager_ptr_);
  return *file_system_manager_ptr_;
}

void FileSystemDispatcher::OpenFileSystem(
    const KURL& origin_url,
    mojom::blink::FileSystemType type,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Open(
      origin_url, type,
      WTF::Bind(&FileSystemDispatcher::DidOpenFileSystem,
                WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::OpenFileSystemSync(
    const KURL& origin_url,
    mojom::blink::FileSystemType type,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  String name;
  KURL root_url;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Open(origin_url, type, &name, &root_url, &error_code);
  DidOpenFileSystem(std::move(callbacks), std::move(name), root_url,
                    error_code);
}

void FileSystemDispatcher::ResolveURL(
    const KURL& filesystem_url,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().ResolveURL(
      filesystem_url,
      WTF::Bind(&FileSystemDispatcher::DidResolveURL, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::ResolveURLSync(
    const KURL& filesystem_url,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  mojom::blink::FileSystemInfoPtr info;
  base::FilePath file_path;
  bool is_directory;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ResolveURL(filesystem_url, &info, &file_path,
                                    &is_directory, &error_code);
  DidResolveURL(std::move(callbacks), std::move(info), std::move(file_path),
                is_directory, error_code);
}

void FileSystemDispatcher::Move(
    const KURL& src_path,
    const KURL& dest_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Move(
      src_path, dest_path,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::MoveSync(
    const KURL& src_path,
    const KURL& dest_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Move(src_path, dest_path, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Copy(
    const KURL& src_path,
    const KURL& dest_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Copy(
      src_path, dest_path,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::CopySync(
    const KURL& src_path,
    const KURL& dest_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Copy(src_path, dest_path, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Remove(
    const KURL& path,
    bool recursive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Remove(
      path, recursive,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::RemoveSync(
    const KURL& path,
    bool recursive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Remove(path, recursive, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::ReadMetadata(
    const KURL& path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().ReadMetadata(
      path, WTF::Bind(&FileSystemDispatcher::DidReadMetadata,
                      WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::ReadMetadataSync(
    const KURL& path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Info file_info;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().ReadMetadata(path, &file_info, &error_code);
  DidReadMetadata(std::move(callbacks), std::move(file_info), error_code);
}

void FileSystemDispatcher::CreateFile(
    const KURL& path,
    bool exclusive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Create(
      path, exclusive, /*is_directory=*/false, /*is_recursive=*/false,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::CreateFileSync(
    const KURL& path,
    bool exclusive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, /*is_directory=*/false,
                                /*is_recursive=*/false, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::CreateDirectory(
    const KURL& path,
    bool exclusive,
    bool recursive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Create(
      path, exclusive, /*is_directory=*/true, recursive,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::CreateDirectorySync(
    const KURL& path,
    bool exclusive,
    bool recursive,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Create(path, exclusive, /*is_directory=*/true,
                                recursive, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::Exists(
    const KURL& path,
    bool is_directory,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().Exists(
      path, is_directory,
      WTF::Bind(&FileSystemDispatcher::DidFinish, WrapWeakPersistent(this),
                std::move(callbacks)));
}

void FileSystemDispatcher::ExistsSync(
    const KURL& path,
    bool is_directory,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().Exists(path, is_directory, &error_code);
  DidFinish(std::move(callbacks), error_code);
}

void FileSystemDispatcher::ReadDirectory(
    const KURL& path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  mojom::blink::FileSystemOperationListenerPtr ptr;
  mojom::blink::FileSystemOperationListenerRequest request =
      mojo::MakeRequest(&ptr);
  op_listeners_.AddBinding(
      std::make_unique<ReadDirectoryListener>(std::move(callbacks)),
      std::move(request));
  GetFileSystemManager().ReadDirectory(path, std::move(ptr));
}

void FileSystemDispatcher::ReadDirectorySync(
    const KURL& path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
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
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().ReadMetadata(
      path, WTF::Bind(&FileSystemDispatcher::InitializeFileWriterCallback,
                      WrapWeakPersistent(this), path, std::move(callbacks)));
}

void FileSystemDispatcher::InitializeFileWriterSync(
    const KURL& path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
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
  mojom::blink::FileSystemCancellableOperationPtr op_ptr;
  mojom::blink::FileSystemCancellableOperationRequest op_request =
      mojo::MakeRequest(&op_ptr);
  int operation_id = next_operation_id_++;
  op_ptr.set_connection_error_handler(
      WTF::Bind(&FileSystemDispatcher::RemoveOperationPtr,
                WrapWeakPersistent(this), operation_id));
  cancellable_operations_.insert(operation_id, std::move(op_ptr));
  GetFileSystemManager().Truncate(
      path, offset, std::move(op_request),
      WTF::Bind(&FileSystemDispatcher::DidTruncate, WrapWeakPersistent(this),
                operation_id, std::move(callback)));

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
                                 const String& blob_id,
                                 int64_t offset,
                                 int* request_id_out,
                                 const WriteCallback& success_callback,
                                 StatusCallback error_callback) {
  mojom::blink::FileSystemCancellableOperationPtr op_ptr;
  mojom::blink::FileSystemCancellableOperationRequest op_request =
      mojo::MakeRequest(&op_ptr);
  int operation_id = next_operation_id_++;
  op_ptr.set_connection_error_handler(
      WTF::Bind(&FileSystemDispatcher::RemoveOperationPtr,
                WrapWeakPersistent(this), operation_id));
  cancellable_operations_.insert(operation_id, std::move(op_ptr));

  mojom::blink::FileSystemOperationListenerPtr listener_ptr;
  mojom::blink::FileSystemOperationListenerRequest request =
      mojo::MakeRequest(&listener_ptr);
  op_listeners_.AddBinding(
      std::make_unique<WriteListener>(
          WTF::BindRepeating(&FileSystemDispatcher::DidWrite,
                             WrapWeakPersistent(this), success_callback,
                             operation_id),
          WTF::Bind(&FileSystemDispatcher::WriteErrorCallback,
                    WrapWeakPersistent(this), std::move(error_callback),
                    operation_id)),
      std::move(request));

  GetFileSystemManager().Write(path, blob_id, offset, std::move(op_request),
                               std::move(listener_ptr));

  if (request_id_out)
    *request_id_out = operation_id;
}

void FileSystemDispatcher::WriteSync(const KURL& path,
                                     const String& blob_id,
                                     int64_t offset,
                                     const WriteCallback& success_callback,
                                     StatusCallback error_callback) {
  int64_t byte_count;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  GetFileSystemManager().WriteSync(path, blob_id, offset, &byte_count,
                                   &error_code);
  if (error_code == base::File::FILE_OK)
    std::move(success_callback).Run(byte_count, /*complete=*/true);
  else
    std::move(error_callback).Run(error_code);
}

void FileSystemDispatcher::Cancel(int request_id_to_cancel,
                                  StatusCallback callback) {
  if (cancellable_operations_.find(request_id_to_cancel) ==
      cancellable_operations_.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  cancellable_operations_.find(request_id_to_cancel)
      ->value->Cancel(WTF::Bind(&FileSystemDispatcher::DidCancel,
                                WrapWeakPersistent(this), std::move(callback),
                                request_id_to_cancel));
}

void FileSystemDispatcher::CreateSnapshotFile(
    const KURL& file_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  GetFileSystemManager().CreateSnapshotFile(
      file_path, WTF::Bind(&FileSystemDispatcher::DidCreateSnapshotFile,
                           WrapWeakPersistent(this), std::move(callbacks)));
}

void FileSystemDispatcher::CreateSnapshotFileSync(
    const KURL& file_path,
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks) {
  base::File::Info file_info;
  base::FilePath platform_path;
  base::File::Error error_code = base::File::FILE_ERROR_FAILED;
  mojom::blink::ReceivedSnapshotListenerPtr listener;
  GetFileSystemManager().CreateSnapshotFile(
      file_path, &file_info, &platform_path, &error_code, &listener);
  DidCreateSnapshotFile(std::move(callbacks), std::move(file_info),
                        std::move(platform_path), error_code,
                        std::move(listener));
}

void FileSystemDispatcher::DidOpenFileSystem(
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
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
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
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

void FileSystemDispatcher::DidFinish(
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK)
    callbacks->DidSucceed();
  else
    callbacks->DidFail(error_code);
}

void FileSystemDispatcher::DidReadMetadata(
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
    const base::File::Info& file_info,
    base::File::Error error_code) {
  if (error_code == base::File::Error::FILE_OK) {
    callbacks->DidReadMetadata(FileMetadata::From(file_info));
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::DidReadDirectory(
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
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
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
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
    RemoveOperationPtr(operation_id);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::DidWrite(const WriteCallback& callback,
                                    int operation_id,
                                    int64_t bytes,
                                    bool complete) {
  callback.Run(bytes, complete);
  if (complete)
    RemoveOperationPtr(operation_id);
}

void FileSystemDispatcher::WriteErrorCallback(StatusCallback callback,
                                              int operation_id,
                                              base::File::Error error) {
  std::move(callback).Run(error);
  if (error != base::File::FILE_ERROR_ABORT)
    RemoveOperationPtr(operation_id);
}

void FileSystemDispatcher::DidCancel(StatusCallback callback,
                                     int cancelled_operation_id,
                                     base::File::Error error_code) {
  if (error_code == base::File::FILE_OK)
    RemoveOperationPtr(cancelled_operation_id);
  std::move(callback).Run(error_code);
}

void FileSystemDispatcher::DidCreateSnapshotFile(
    std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    base::File::Error error_code,
    mojom::blink::ReceivedSnapshotListenerPtr listener) {
  if (error_code == base::File::FILE_OK) {
    FileMetadata file_metadata = FileMetadata::From(file_info);
    file_metadata.platform_path = FilePathToWebString(platform_path);

    std::unique_ptr<BlobData> blob_data = BlobData::Create();
    blob_data->AppendFile(file_metadata.platform_path, 0, file_metadata.length,
                          InvalidFileTime());
    scoped_refptr<BlobDataHandle> snapshot_blob =
        BlobDataHandle::Create(std::move(blob_data), file_metadata.length);

    callbacks->DidCreateSnapshotFile(file_metadata, snapshot_blob);

    if (listener)
      listener->DidReceiveSnapshotFile();
  } else {
    callbacks->DidFail(error_code);
  }
}

void FileSystemDispatcher::RemoveOperationPtr(int operation_id) {
  DCHECK(cancellable_operations_.find(operation_id) !=
         cancellable_operations_.end());
  cancellable_operations_.erase(operation_id);
}

}  // namespace blink
