// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DISPATCHER_H_

#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/platform/async_file_system_callbacks.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace WTF {
class String;
}

namespace blink {

class KURL;
class ExecutionContext;

// Sends messages via mojo to the blink::mojom::FileSystemManager service
// running in the browser process. It is owned by ExecutionContext, and
// instances are created lazily by calling FileSystemDispatcher::From().
class FileSystemDispatcher
    : public GarbageCollectedFinalized<FileSystemDispatcher>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(FileSystemDispatcher);

 public:
  using StatusCallback = base::OnceCallback<void(base::File::Error error)>;
  using WriteCallback =
      base::RepeatingCallback<void(int64_t bytes, bool complete)>;

  static const char kSupplementName[];

  static FileSystemDispatcher& From(ExecutionContext* context);
  virtual ~FileSystemDispatcher();

  mojom::blink::FileSystemManager& GetFileSystemManager();

  void OpenFileSystem(const KURL& url,
                      mojom::blink::FileSystemType type,
                      std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void OpenFileSystemSync(const KURL& url,
                          mojom::blink::FileSystemType type,
                          std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void ResolveURL(const KURL& filesystem_url,
                  std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void ResolveURLSync(const KURL& filesystem_url,
                      std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void Move(const KURL& src_path,
            const KURL& dest_path,
            std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void MoveSync(const KURL& src_path,
                const KURL& dest_path,
                std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void Copy(const KURL& src_path,
            const KURL& dest_path,
            std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void CopySync(const KURL& src_path,
                const KURL& dest_path,
                std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void Remove(const KURL& path,
              bool recursive,
              std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void RemoveSync(const KURL& path,
                  bool recursive,
                  std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void ReadMetadata(const KURL& path,
                    std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void ReadMetadataSync(const KURL& path,
                        std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void CreateFile(const KURL& path,
                  bool exclusive,
                  std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void CreateFileSync(const KURL& path,
                      bool exclusive,
                      std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void CreateDirectory(const KURL& path,
                       bool exclusive,
                       bool recursive,
                       std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void CreateDirectorySync(const KURL& path,
                           bool exclusive,
                           bool recursive,
                           std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void Exists(const KURL& path,
              bool for_directory,
              std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void ExistsSync(const KURL& path,
                  bool for_directory,
                  std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void ReadDirectory(const KURL& path,
                     std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void ReadDirectorySync(const KURL& path,
                         std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

  void InitializeFileWriter(const KURL& path,
                            std::unique_ptr<AsyncFileSystemCallbacks>);
  void InitializeFileWriterSync(const KURL& path,
                                std::unique_ptr<AsyncFileSystemCallbacks>);

  void Truncate(const KURL& path,
                int64_t offset,
                int* request_id_out,
                StatusCallback callback);
  void TruncateSync(const KURL& path, int64_t offset, StatusCallback callback);

  void Write(const KURL& path,
             const String& blob_id,
             int64_t offset,
             int* request_id_out,
             const WriteCallback& success_callback,
             StatusCallback error_callback);
  void WriteSync(const KURL& path,
                 const String& blob_id,
                 int64_t offset,
                 const WriteCallback& success_callback,
                 StatusCallback error_callback);

  void Cancel(int request_id_to_cancel, StatusCallback callback);

  void CreateSnapshotFile(const KURL& file_path,
                          std::unique_ptr<AsyncFileSystemCallbacks> callbacks);
  void CreateSnapshotFileSync(
      const KURL& file_path,
      std::unique_ptr<AsyncFileSystemCallbacks> callbacks);

 private:
  class WriteListener;
  class ReadDirectoryListener;

  explicit FileSystemDispatcher(ExecutionContext& context);

  void DidOpenFileSystem(std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
                         const String& name,
                         const KURL& root,
                         base::File::Error error_code);
  void DidResolveURL(std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
                     mojom::blink::FileSystemInfoPtr info,
                     const base::FilePath& file_path,
                     bool is_directory,
                     base::File::Error error_code);
  void DidFinish(std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
                 base::File::Error error_code);
  void DidReadMetadata(std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
                       const base::File::Info& file_info,
                       base::File::Error error);
  void DidReadDirectory(
      std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
      Vector<filesystem::mojom::blink::DirectoryEntryPtr> entries,
      base::File::Error error_code);
  void InitializeFileWriterCallback(
      const KURL& path,
      std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
      const base::File::Info& file_info,
      base::File::Error error);
  void DidTruncate(int operation_id,
                   StatusCallback callback,
                   base::File::Error error_code);
  void DidWrite(const WriteCallback& callback,
                int operation_id,
                int64_t bytes,
                bool complete);
  void WriteErrorCallback(StatusCallback callback,
                          int operation_id,
                          base::File::Error error);
  void DidCancel(StatusCallback callback,
                 int cancelled_operation_id,
                 base::File::Error error_code);
  void DidCreateSnapshotFile(
      std::unique_ptr<AsyncFileSystemCallbacks> callbacks,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      base::File::Error error_code,
      mojom::blink::ReceivedSnapshotListenerPtr listener);

  void RemoveOperationPtr(int operation_id);

  mojom::blink::FileSystemManagerPtr file_system_manager_ptr_;
  using OperationsMap =
      HashMap<int, mojom::blink::FileSystemCancellableOperationPtr>;
  OperationsMap cancellable_operations_;
  int next_operation_id_;
  mojo::StrongBindingSet<mojom::blink::FileSystemOperationListener>
      op_listeners_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DISPATCHER_H_
