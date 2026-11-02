// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_IMPL_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_IMPL_H_

#include <optional>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/cross_origin_storage/cross_origin_storage.mojom.h"
#include "url/origin.h"

namespace content {

class ChromeBlobStorageContext;
class FileSystemAccessManagerImpl;
class RenderFrameHost;
class RenderProcessHost;

// Serves the Cross-Origin Storage Mojo interface for a single renderer
// execution context (a frame or a worker).
//
// This object holds the caller's `StorageKey`, which the renderer cannot
// influence, and is therefore the security boundary for the API: every request
// is evaluated against the storage key recorded here rather than anything
// supplied in the message.
class CONTENT_EXPORT CrossOriginStorageManagerImpl
    : public blink::mojom::CrossOriginStorageManager,
      public base::SupportsUserData::Data {
 public:
  // Binds a document-scoped instance (`DocumentService`) for `frame_host`. Does
  // nothing if the process is not permitted to access data for the frame's
  // origin, if `StoragePartition` is non-default or in-memory, or if the
  // profile has no Cross-Origin Storage cache (for example, off-the-record
  // profiles).
  static void BindForFrame(
      RenderFrameHost* frame_host,
      mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver);

  // Binds `receiver` to a `worker_host`-owned `ReceiverSet` for `storage_key`.
  // Does nothing if `process_host` is not permitted to access data for
  // `storage_key.origin()`, if `StoragePartition` is non-default or in-memory,
  // or if the profile has no Cross-Origin Storage cache.
  static void BindForWorker(
      base::SupportsUserData* worker_host,
      RenderProcessHost* process_host,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver);

  CrossOriginStorageManagerImpl(
      base::WeakPtr<CrossOriginStorageRegistry> registry,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
      base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager,
      blink::StorageKey storage_key);
  CrossOriginStorageManagerImpl(const CrossOriginStorageManagerImpl&) = delete;
  CrossOriginStorageManagerImpl& operator=(
      const CrossOriginStorageManagerImpl&) = delete;
  ~CrossOriginStorageManagerImpl() override;

  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver);

  // Validates the request and invokes `callback` on normal completion. If the
  // Mojo arguments are malformed, leaves `callback` uninvoked and returns the
  // bad-message string so the caller can report and close its receiver before
  // `callback` is destroyed.
  std::optional<std::string_view> HandleGetFileHandle(
      blink::mojom::CrossOriginStorageHashPtr hash,
      blink::mojom::CrossOriginStorageOriginsPtr create_origins,
      GetFileHandleCallback& callback);

  // blink::mojom::CrossOriginStorageManager:
  void GetFileHandle(blink::mojom::CrossOriginStorageHashPtr hash,
                     blink::mojom::CrossOriginStorageOriginsPtr create_origins,
                     GetFileHandleCallback callback) override;

 private:
  void OnOpenedForRead(CrossOriginStorageHash hash,
                       GetFileHandleCallback callback,
                       CrossOriginStorageOpenResult result);

  const base::WeakPtr<CrossOriginStorageRegistry> registry_;
  const scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  const base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager_;

  // The storage key of the execution context this instance serves.
  const blink::StorageKey storage_key_;

  mojo::ReceiverSet<blink::mojom::CrossOriginStorageManager> receivers_;

  base::WeakPtrFactory<CrossOriginStorageManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_MANAGER_IMPL_H_
