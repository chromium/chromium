// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemURL;
struct FileSystemInfo;
class ShareableFileReference;
}  // namespace storage

namespace content {
class ChromeBlobStorageContext;

// All methods for this class are expected to be called on the IO thread,
// except for the constructor. The destructor must also be called on the IO
// thread as weak refs are created on that thread. A single instance of this
// class is owned by RenderProcessHostImpl.
class CONTENT_EXPORT FileSystemManagerImpl
    : public blink::mojom::FileSystemManager {
 public:
  // Constructed and held by the RenderFrameHost and render process host on
  // the UI thread. Used by render frames (via the RenderFrameHost), workers
  // and pepper (via the render process host).
  FileSystemManagerImpl(
      int process_id,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context);

  FileSystemManagerImpl(const FileSystemManagerImpl&) = delete;
  FileSystemManagerImpl& operator=(const FileSystemManagerImpl&) = delete;

  ~FileSystemManagerImpl() override;
  base::WeakPtr<FileSystemManagerImpl> GetWeakPtr();

  void BindReceiver(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver);

  // blink::mojom::FileSystem
  void Open(const url::Origin& origin,
            blink::mojom::FileSystemType file_system_type,
            OpenCallback callback) override;
  void ResolveURL(const GURL& filesystem_url,
                  ResolveURLCallback callback) override;
  void Move(const GURL& src_path,
            const GURL& dest_path,
            MoveCallback callback) override;
  void Copy(const GURL& src_path,
            const GURL& dest_path,
            CopyCallback callback) override;
  void Remove(const GURL& path,
              bool recursive,
              RemoveCallback callback) override;
  void ReadMetadata(const GURL& path, ReadMetadataCallback callback) override;
  void Create(const GURL& path,
              bool exclusive,
              bool is_directory,
              bool recursive,
              CreateCallback callback) override;
  void Exists(const GURL& path,
              bool is_directory,
              ExistsCallback callback) override;
  void ReadDirectory(
      const GURL& path,
      mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
          pending_listener) override;
  void ReadDirectorySync(const GURL& path,
                         ReadDirectorySyncCallback callback) override;
  void Write(const GURL& file_path,
             mojo::PendingRemote<blink::mojom::Blob> blob,
             int64_t position,
             mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
                 op_receiver,
             mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
                 pending_listener) override;
  void WriteSync(const GURL& file_path,
                 mojo::PendingRemote<blink::mojom::Blob> blob,
                 int64_t position,
                 WriteSyncCallback callback) override;
  void Truncate(
      const GURL& file_path,
      int64_t length,
      mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
          op_receiver,
      TruncateCallback callback) override;
  void TruncateSync(const GURL& file_path,
                    int64_t length,
                    TruncateSyncCallback callback) override;
  void CreateSnapshotFile(const GURL& file_path,
                          CreateSnapshotFileCallback callback) override;
  void GetPlatformPath(const GURL& file_path,
                       GetPlatformPathCallback callback) override;
  void RegisterBlob(const std::string& content_type,
                    const GURL& url,
                    uint64_t length,
                    std::optional<base::Time> expected_modification_time,
                    RegisterBlobCallback callback) override;

 private:
  class FileSystemCancellableOperationImpl;
  class ReceivedSnapshotListenerImpl;
  using OperationID = storage::FileSystemOperationRunner::OperationID;
  using OperationListenerID = int;
  struct WriteSyncCallbackEntry;
  struct ReadDirectorySyncCallbackEntry;

  void ContinueOpen(const url::Origin& origin,
                    blink::mojom::FileSystemType file_system_type,
                    mojo::ReportBadMessageCallback bad_message_callback,
                    OpenCallback callback,
                    const blink::StorageKey& storage_key,
                    bool security_check_success);
  void ContinueResolveURL(const storage::FileSystemURL& url,
                          ResolveURLCallback callback,
                          bool security_check_success);
  void ContinueMove(const storage::FileSystemURL& src_url,
                    const storage::FileSystemURL& dest_url,
                    MoveCallback callback,
                    bool security_check_success);
  void ContinueCopy(const storage::FileSystemURL& src_url,
                    const storage::FileSystemURL& dest_url,
                    CopyCallback callback,
                    bool security_check_success);
  void ContinueRemove(const storage::FileSystemURL& url,
                      bool recursive,
                      RemoveCallback callback,
                      bool security_check_success);
  void ContinueReadMetadata(const storage::FileSystemURL& url,
                            ReadMetadataCallback callback,
                            bool security_check_success);
  void ContinueCreate(const storage::FileSystemURL& url,
                      bool exclusive,
                      bool is_directory,
                      bool recursive,
                      CreateCallback callback,
                      bool security_check_success);
  void ContinueExists(const storage::FileSystemURL& url,
                      bool is_directory,
                      ExistsCallback callback,
                      bool security_check_success);
  void ContinueReadDirectory(
      const storage::FileSystemURL& url,
      mojo::Remote<blink::mojom::FileSystemOperationListener> listener,
      bool security_check_success);
  void ContinueReadDirectorySync(const storage::FileSystemURL& url,
                                 ReadDirectorySyncCallback callback,
                                 bool security_check_success);
  void ResolveBlobForWrite(
      mojo::PendingRemote<blink::mojom::Blob> blob,
      base::OnceCallback<void(std::unique_ptr<storage::BlobDataHandle>)>
          callback,
      bool security_check_success);
  void ContinueWrite(
      const storage::FileSystemURL& url,
      int64_t position,
      mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
          op_receiver,
      mojo::Remote<blink::mojom::FileSystemOperationListener> listener,
      std::unique_ptr<storage::BlobDataHandle> blob);
  void ContinueWriteSync(const storage::FileSystemURL& url,
                         int64_t position,
                         WriteSyncCallback callback,
                         std::unique_ptr<storage::BlobDataHandle> blob);
  void ContinueTruncate(
      const storage::FileSystemURL& url,
      int64_t length,
      mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
          op_receiver,
      TruncateCallback callback,
      bool security_check_success);
  void ContinueTruncateSync(const storage::FileSystemURL& url,
                            int64_t length,
                            TruncateSyncCallback callback,
                            bool security_check_success);
  void ContinueCreateSnapshotFile(const storage::FileSystemURL& url,
                                  CreateSnapshotFileCallback callback,
                                  bool security_check_success);
  void ContinueRegisterBlob(
      const std::string& content_type,
      const GURL& url,
      uint64_t length,
      std::optional<base::Time> expected_modification_time,
      RegisterBlobCallback callback,
      storage::FileSystemURL crack_url,
      bool security_check_success);

  void Cancel(
      OperationID op_id,
      blink::mojom::FileSystemCancellableOperation::CancelCallback callback);
  void DidReceiveSnapshotFile(int snapshot_id);
  void OnConnectionError();

  // Callback functions to be used when each file operation is finished.
  void DidFinish(base::OnceCallback<void(base::File::Error)> callback,
                 base::File::Error error_code);
  void DidGetMetadata(ReadMetadataCallback callback,
                      base::File::Error result,
                      const base::File::Info& info);
  void DidGetMetadataForStreaming(CreateSnapshotFileCallback callback,
                                  base::File::Error result,
                                  const base::File::Info& info);
  void DidReadDirectory(OperationListenerID listener_id,
                        base::File::Error result,
                        std::vector<filesystem::mojom::DirectoryEntry> entries,
                        bool has_more);
  void DidReadDirectorySync(
      ReadDirectorySyncCallbackEntry* callback_entry,
      base::File::Error result,
      std::vector<filesystem::mojom::DirectoryEntry> entries,
      bool has_more);
  void DidWrite(OperationListenerID listener_id,
                base::File::Error result,
                int64_t bytes,
                bool complete);
  void DidWriteSync(WriteSyncCallbackEntry* entry,
                    base::File::Error result,
                    int64_t bytes,
                    bool complete);
  void DidOpenFileSystem(OpenCallback callback,
                         const storage::FileSystemURL& root,
                         const std::string& filesystem_name,
                         base::File::Error result);
  void DidResolveURL(ResolveURLCallback callback,
                     base::File::Error result,
                     const storage::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     storage::FileSystemContext::ResolvedEntryType type);
  void DidCreateSnapshot(
      CreateSnapshotFileCallback callback,
      const storage::FileSystemURL& url,
      base::File::Error result,
      const base::File::Info& info,
      const base::FilePath& platform_path,
      scoped_refptr<storage::ShareableFileReference> file_ref);
  void ContinueDidCreateSnapshot(CreateSnapshotFileCallback callback,
                                 const storage::FileSystemURL& url,
                                 base::File::Error result,
                                 const base::File::Info& info,
                                 const base::FilePath& platform_path,
                                 bool security_check_success);
  void DidGetPlatformPath(scoped_refptr<storage::FileSystemContext> context,
                          GetPlatformPathCallback callback,
                          base::FilePath platform_path);
  static void GetPlatformPathOnFileThread(
      const GURL& path,
      int process_id,
      scoped_refptr<storage::FileSystemContext> context,
      base::WeakPtr<FileSystemManagerImpl> file_system_manager,
      const blink::StorageKey& storage_key,
      GetPlatformPathCallback callback);
  // Returns an error if `url` is invalid.
  std::optional<base::File::Error> ValidateFileSystemURL(
      const storage::FileSystemURL& url);

  storage::FileSystemOperationRunner* operation_runner() {
    return operation_runner_.get();
  }

  OperationListenerID AddOpListener(
      mojo::Remote<blink::mojom::FileSystemOperationListener> listener);
  void RemoveOpListener(OperationListenerID listener_id);
  blink::mojom::FileSystemOperationListener* GetOpListener(
      OperationListenerID listener_id);
  void OnConnectionErrorForOpListeners(OperationListenerID listener_id);

  const int process_id_;
  const scoped_refptr<storage::FileSystemContext> context_;
  const scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  std::unique_ptr<storage::FileSystemOperationRunner> operation_runner_;

  mojo::ReceiverSet<blink::mojom::FileSystemManager, blink::StorageKey>
      receivers_;
  mojo::UniqueReceiverSet<blink::mojom::FileSystemCancellableOperation>
      cancellable_operations_;
  mojo::UniqueReceiverSet<blink::mojom::ReceivedSnapshotListener>
      snapshot_listeners_;

  std::unordered_map<OperationListenerID,
                     mojo::Remote<blink::mojom::FileSystemOperationListener>>
      op_listeners_;
  OperationListenerID next_operation_listener_id_ = 1;

  // Used to keep snapshot files alive while a DidCreateSnapshot
  // is being sent to the renderer.
  base::IDMap<scoped_refptr<storage::ShareableFileReference>>
      in_transit_snapshot_files_;

  base::WeakPtrFactory<FileSystemManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_MANAGER_IMPL_H_
