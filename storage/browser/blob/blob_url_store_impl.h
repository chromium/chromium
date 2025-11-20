// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_STORE_IMPL_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_STORE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace storage {

class BlobUrlRegistry;

class COMPONENT_EXPORT(STORAGE_BROWSER) BlobURLStoreImpl
    : public blink::mojom::BlobURLStore {
 public:
  // `partitioning_blob_url_closure` runs when the storage_key check fails
  // in `BlobURLStoreImpl::ResolveAsURLLoaderFactory`.
  BlobURLStoreImpl(
      const blink::StorageKey& storage_key,
      const url::Origin& renderer_origin,
      int render_process_host_id,
      base::WeakPtr<BlobUrlRegistry> registry,
      BlobURLValidityCheckBehavior validity_check_options =
          BlobURLValidityCheckBehavior::DEFAULT,
      base::RepeatingCallback<
          void(const GURL&,
               std::optional<blink::mojom::PartitioningBlobURLInfo>)>
          partitioning_blob_url_closure = base::DoNothing(),
      base::RepeatingCallback<bool()> storage_access_check_closure =
          base::BindRepeating([]() -> bool { return false; }),
      std::optional<GURL> top_level_blob_document_url = std::nullopt,
      bool partitioning_disabled_by_policy = false,
      const char* context_type_for_debugging = "",
      base::RepeatingCallback<std::string()> storage_key_debug_string_callback =
          base::BindRepeating([]() -> std::string { return ""; }));

  BlobURLStoreImpl(const BlobURLStoreImpl&) = delete;
  BlobURLStoreImpl& operator=(const BlobURLStoreImpl&) = delete;

  ~BlobURLStoreImpl() override;

  void Register(
      mojo::PendingRemote<blink::mojom::Blob> blob,
      const GURL& url,
      RegisterCallback callback) override;
  void Revoke(const GURL& url) override;
  void ResolveAsURLLoaderFactory(
      const GURL& url,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  void ResolveAsBlobURLToken(
      const GURL& url,
      mojo::PendingReceiver<blink::mojom::BlobURLToken> token,
      bool is_top_level_navigation) override;

 private:
  // Checks if the passed in url is a valid blob url for this blob url store.
  // Returns false and reports a bad mojo message if not. Note that currently
  // this function is only suitable to be called from `Register()` and
  // `Revoke()`.
  bool BlobUrlIsValid(const GURL& url, const char* method) const;

  bool ShouldPartitionBlobUrlAccess(
      bool has_storage_access_handle,
      BlobUrlRegistry::MappingStatus mapping_status);

  const blink::StorageKey storage_key_;
  // The origin used by the worker/document associated with this BlobURLStore on
  // the renderer side. This will almost always be the same as `storage_key_`'s
  // origin, except in the case of data: URL workers, as described in the linked
  //  bug.
  // TODO(crbug.com/40051700): Make the storage key's origin always match this.
  const url::Origin renderer_origin_;
  const int render_process_host_id_;

  base::WeakPtr<BlobUrlRegistry> registry_;

  const BlobURLValidityCheckBehavior validity_check_behavior_;

  std::set<GURL> urls_;

  base::RepeatingCallback<
      void(const GURL&, std::optional<blink::mojom::PartitioningBlobURLInfo>)>
      partitioning_blob_url_closure_;

  base::RepeatingCallback<bool()> storage_access_check_callback_;

  // Set when this BlobURLStoreImpl corresponds to a top-level document created
  // by navigating to a blob URL.
  std::optional<GURL> top_level_blob_document_url_;

  const bool partitioning_disabled_by_policy_;

  // TODO(crbug.com/417149687): Remove these once we've collected enough
  // data to gauge the conditions under which origin mismatch crashes occur.
  std::string context_type_for_debugging_;
  base::RepeatingCallback<std::string()> storage_key_debug_string_callback_;

  base::WeakPtrFactory<BlobURLStoreImpl> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_STORE_IMPL_H_
