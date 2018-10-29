// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/browser/streams/stream.h"
#include "content/browser/streams/stream_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_types.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"

class GURL;

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace storage {
class FileSystemURL;
class FileSystemOperationRunner;
struct FileSystemInfo;
class ShareableFileReference;
}  // namespace storage

namespace content {
class ChildProcessSecurityPolicyImpl;
class ChromeBlobStorageContext;

// All methods for this class are expected to be called on the IO thread,
// except for the constructor. The destructor must also be called on the IO
// thread as weak refs are created on that thread. A single instance of this
// class is owned by RenderProcessHostImpl.
class CONTENT_EXPORT FileSystemManagerImpl
    : public blink::mojom::FileSystemManager {
 public:
  // Constructed and held by the render frame host and render process host on
  // the UI thread. Used by render frames (via the render frame host), workers
  // and pepper (via the render process host).
  FileSystemManagerImpl(
      int process_id,
      int frame_id,
      storage::FileSystemContext* file_system_context,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context);
  ~FileSystemManagerImpl() override;
  base::WeakPtr<FileSystemManagerImpl> GetWeakPtr();

  void BindRequest(blink::mojom::FileSystemManagerRequest request);

  // blink::mojom::FileSystem
  void Open(const GURL& origin_url,
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
      blink::mojom::FileSystemOperationListenerPtr listener) override;
  void ReadDirectorySync(const GURL& path,
                         ReadDirectorySyncCallback callback) override;
  void Write(const GURL& file_path,
             const std::string& blob_uuid,
             int64_t position,
             blink::mojom::FileSystemCancellableOperationRequest op_request,
             blink::mojom::FileSystemOperationListenerPtr listener) override;
  void WriteSync(const GURL& file_path,
                 const std::string& blob_uuid,
                 int64_t position,
                 WriteSyncCallback callback) override;
  void Truncate(const GURL& file_path,
                int64_t length,
                blink::mojom::FileSystemCancellableOperationRequest op_request,
                TruncateCallback callback) override;
  void TruncateSync(const GURL& file_path,
                    int64_t length,
                    TruncateSyncCallback callback) override;
  void TouchFile(const GURL& path,
                 base::Time last_access_time,
                 base::Time last_modified_time,
                 TouchFileCallback callback) override;
  void CreateSnapshotFile(const GURL& file_path,
                          CreateSnapshotFileCallback callback) override;
  void GetPlatformPath(const GURL& file_path,
                       GetPlatformPathCallback callback) override;
  void CreateWriter(const GURL& file_path,
                    CreateWriterCallback callback) override;
  void ChooseEntry(
      blink::mojom::ChooseFileSystemEntryType type,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ChooseEntryCallback callback) override;

 private:
  class FileSystemCancellableOperationImpl;
  class ReceivedSnapshotListenerImpl;
  using OperationID = storage::FileSystemOperationRunner::OperationID;
  using OperationListenerID = int;
  struct WriteSyncCallbackEntry;
  struct ReadDirectorySyncCallbackEntry;

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
                         const GURL& root,
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
  void DidGetPlatformPath(GetPlatformPathCallback callback,
                          base::FilePath platform_path);

  static void GetPlatformPathOnFileThread(
      const GURL& path,
      int process_id,
      storage::FileSystemContext* context,
      base::WeakPtr<FileSystemManagerImpl> file_system_manager,
      GetPlatformPathCallback callback);
  // Returns an error if |url| is invalid.
  base::Optional<base::File::Error> ValidateFileSystemURL(
      const storage::FileSystemURL& url);

  storage::FileSystemOperationRunner* operation_runner() {
    return operation_runner_.get();
  }

  OperationListenerID AddOpListener(
      blink::mojom::FileSystemOperationListenerPtr listener);
  void RemoveOpListener(OperationListenerID listener_id);
  blink::mojom::FileSystemOperationListener* GetOpListener(
      OperationListenerID listener_id);
  void OnConnectionErrorForOpListeners(OperationListenerID listener_id);

  const int process_id_;
  const int frame_id_;
  storage::FileSystemContext* const context_;
  ChildProcessSecurityPolicyImpl* const security_policy_;
  const scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  std::unique_ptr<storage::FileSystemOperationRunner> operation_runner_;

  mojo::BindingSet<blink::mojom::FileSystemManager> bindings_;
  mojo::StrongBindingSet<blink::mojom::FileSystemCancellableOperation>
      cancellable_operations_;
  mojo::StrongBindingSet<blink::mojom::ReceivedSnapshotListener>
      snapshot_listeners_;

  std::unordered_map<OperationListenerID,
                     blink::mojom::FileSystemOperationListenerPtr>
      op_listeners_;
  OperationListenerID next_operation_listener_id_ = 1;

  // Used to keep snapshot files alive while a DidCreateSnapshot
  // is being sent to the renderer.
  base::IDMap<scoped_refptr<storage::ShareableFileReference>>
      in_transit_snapshot_files_;

  base::WeakPtrFactory<FileSystemManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_MANAGER_IMPL_H_
