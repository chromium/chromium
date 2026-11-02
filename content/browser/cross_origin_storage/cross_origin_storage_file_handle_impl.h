// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_HANDLE_IMPL_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_access_handle_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"
#include "url/origin.h"

namespace content {

class ChromeBlobStorageContext;
struct CrossOriginStorageOpenResult;
class CrossOriginStorageRegistry;
class CrossOriginStorageStagingWriter;
class FileSystemAccessManagerImpl;
class FileSystemAccessTransferTokenImpl;

// Browser-process implementation of `FileSystemAccessFileHandle` for
// Cross-Origin Storage.
//
// Mediates access to the content-addressed storage backend for a specific
// cryptographic hash and origin, enforcing write staging isolation and
// preventing uncommitted handles from acting as existence oracles.
class CONTENT_EXPORT CrossOriginStorageFileHandleImpl
    : public blink::mojom::FileSystemAccessFileHandle {
 public:
  CrossOriginStorageFileHandleImpl(
      base::WeakPtr<CrossOriginStorageRegistry> registry,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
      base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager,
      CrossOriginStorageHash hash,
      url::Origin origin,
      CrossOriginStorageAccessSpec access_spec,
      bool is_writable,
      bool may_read);

  CrossOriginStorageFileHandleImpl(const CrossOriginStorageFileHandleImpl&) =
      delete;
  CrossOriginStorageFileHandleImpl& operator=(
      const CrossOriginStorageFileHandleImpl&) = delete;
  ~CrossOriginStorageFileHandleImpl() override;

  base::WeakPtr<CrossOriginStorageRegistry> registry() const {
    return registry_;
  }
  const scoped_refptr<ChromeBlobStorageContext>& blob_storage_context() const {
    return blob_storage_context_;
  }
  const CrossOriginStorageHash& hash() const { return hash_; }
  const url::Origin& origin() const { return origin_; }
  const CrossOriginStorageAccessSpec& access_spec() const {
    return access_spec_;
  }
  bool is_writable() const { return is_writable_; }
  bool may_read() const { return may_read_; }

  // blink::mojom::FileSystemAccessFileHandle:
  void GetPermissionStatus(blink::mojom::FileSystemAccessPermissionMode mode,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(blink::mojom::FileSystemAccessPermissionMode mode,
                         RequestPermissionCallback callback) override;

  // Reads the committed blob only when `may_read_` is true (handles obtained
  // via read that cleared availability gating, or write handles after a write
  // commits), preventing `{create: true}` from acting as an existence oracle.
  void AsBlob(AsBlobCallback callback) override;

  // Produces a writable stream backed by an append-only quarantine file in
  // `staging/`. Only permitted on handles created with `{create: true}`
  // (`is_writable_`).
  void CreateFileWriter(
      bool keep_existing_data,
      bool auto_close,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode mode,
      CreateFileWriterCallback callback) override;

  // Mutation and direct OS file descriptor access are rejected to protect
  // content-addressed immutability across origins.
  void Move(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                destination_directory,
            const std::string& new_entry_name,
            MoveCallback callback) override;
  void Rename(const std::string& new_entry_name,
              RenameCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void OpenAccessHandle(blink::mojom::FileSystemAccessAccessHandleLockMode mode,
                        OpenAccessHandleCallback callback) override;
  void IsSameEntry(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> other,
      IsSameEntryCallback callback) override;
  void Transfer(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token)
      override;
  void GetUniqueId(GetUniqueIdCallback callback) override;
  void GetCloudIdentifiers(GetCloudIdentifiersCallback callback) override;

 private:
  void OnStagingWriterCreated(
      CreateFileWriterCallback callback,
      base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer);
  void OnWriteCommitted();
  void OnOpenedForRead(AsBlobCallback callback,
                       CrossOriginStorageOpenResult result);
  void IsSameEntryImpl(IsSameEntryCallback callback,
                       FileSystemAccessTransferTokenImpl* other);

  const base::WeakPtr<CrossOriginStorageRegistry> registry_;
  const scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  const base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager_;
  const CrossOriginStorageHash hash_;
  const url::Origin origin_;
  const CrossOriginStorageAccessSpec access_spec_;
  const bool is_writable_;
  bool may_read_;

  base::WeakPtrFactory<CrossOriginStorageFileHandleImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_FILE_HANDLE_IMPL_H_
