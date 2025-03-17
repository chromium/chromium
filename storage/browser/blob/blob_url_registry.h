// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_

#include <map>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

class GURL;

namespace storage {

// This class stores the mapping of blob Urls to blobs.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobUrlRegistry {
 public:
  explicit BlobUrlRegistry(base::WeakPtr<BlobUrlRegistry> fallback = nullptr);

  BlobUrlRegistry(const BlobUrlRegistry&) = delete;
  BlobUrlRegistry& operator=(const BlobUrlRegistry&) = delete;

  ~BlobUrlRegistry();

  enum class MappingStatus {
    kIsMapped,
    // TODO(crbug.com/387655548): Remove this case once there's sufficient data
    // from the CrossPartitionSameOriginBlobURLFetch UseCounter. Currently, this
    // case is treated separately because cross-origin Blob URL access is
    // already blocked and shouldn't be measured w.r.t. deciding whether it's
    // safe to restrict further based on storage partition. Once
    // CrossPartitionSameOriginBlobURLFetch is removed, it'd be
    // beneficial to show the DevTools Issue even in the cross-origin access
    // case and simplify IsUrlMapped to return a bool.
    kNotMappedCrossPartitionSameOrigin,
    kNotMappedOther
  };

  // Binds receivers corresponding to connections from renderer frame
  // contexts and stores them in `frame_receivers_`.
  // `partitioning_blob_url_closure` runs when the storage_key check fails
  // in `BlobURLStoreImpl::ResolveAsURLLoaderFactory` and increments the use
  // counter.
  void AddReceiver(
      const blink::StorageKey& storage_key,
      const url::Origin& renderer_origin,
      int render_process_host_id,
      mojo::PendingAssociatedReceiver<blink::mojom::BlobURLStore> receiver,
      base::RepeatingCallback<
          void(const GURL&,
               std::optional<blink::mojom::PartitioningBlobURLInfo>)>
          partitioning_blob_url_closure,
      bool partitioning_disabled_by_policy = false);

  // Binds receivers corresponding to connections from renderer worker
  // contexts and stores them in `worker_receivers_`.
  void AddReceiver(const blink::StorageKey& storage_key,
                   const url::Origin& renderer_origin,
                   int render_process_host_id,
                   mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver,
                   bool partitioning_disabled_by_policy = false,
                   BlobURLValidityCheckBehavior validity_check_behavior =
                       BlobURLValidityCheckBehavior::DEFAULT);

  // Returns the receivers corresponding to renderer frame contexts for use in
  // tests.
  auto& receivers_for_testing() { return frame_receivers_; }

  // Creates a URL mapping from blob to the given URL. Returns false if
  // there already is a map for the URL. The URL mapping will be associated with
  // the `storage_key`, and most subsequent URL lookup attempts will require a
  // matching StorageKey to succeed. `origin` is the origin of the Blob URL, and
  // `render_process_host_id` is the ID of the process where the blob URL
  // registration comes from.
  bool AddUrlMapping(
      const GURL& url,
      mojo::PendingRemote<blink::mojom::Blob> blob,
      const blink::StorageKey& storage_key,
      const url::Origin& renderer_origin,
      int render_process_host_id,
      // TODO(crbug.com/40775506): Remove these once experiment is over.
      const base::UnguessableToken& unsafe_agent_cluster_id,
      const std::optional<net::SchemefulSite>& unsafe_top_level_site);

  // Removes the given URL mapping associated with `storage_key`. Returns false
  // if the URL wasn't mapped.
  bool RemoveUrlMapping(const GURL& url, const blink::StorageKey& storage_key);

  // Returns whether the URL is mapped to a blob and whether the URL is
  // associated with `storage_key`.
  MappingStatus IsUrlMapped(const GURL& blob_url,
                            const blink::StorageKey& storage_key) const;

  // TODO(crbug.com/40775506): Remove this once experiment is over.
  std::optional<base::UnguessableToken> GetUnsafeAgentClusterID(
      const GURL& blob_url) const;
  std::optional<net::SchemefulSite> GetUnsafeTopLevelSite(
      const GURL& blob_url) const;

  // Returns the blob from the given url. Returns a null remote if the mapping
  // doesn't exist.
  mojo::PendingRemote<blink::mojom::Blob> GetBlobFromUrl(const GURL& url);

  size_t url_count() const { return url_to_blob_.size(); }

  void AddTokenMapping(const base::UnguessableToken& token,
                       const GURL& url,
                       mojo::PendingRemote<blink::mojom::Blob> blob);
  void RemoveTokenMapping(const base::UnguessableToken& token);
  bool GetTokenMapping(const base::UnguessableToken& token,
                       GURL* url,
                       mojo::PendingRemote<blink::mojom::Blob>* blob);

  // Returns the origin for a Blob URL navigation to `url`, given the precursor
  // origin and target process information.
  url::Origin GetOriginForNavigation(
      const GURL& url,
      const url::Origin& precursor_origin,
      std::optional<int> target_render_process_host_id);

  // Support adding a handler to be run when AddReceiver is called. This allows
  // browser tests to intercept incoming BlobURLStore connections and swap in
  // arbitrary BlobURLs to ensure that attempting to register certain blobs
  // causes the renderer to be terminated.
  using URLStoreCreationHook =
      base::RepeatingCallback<void(BlobUrlRegistry*, mojo::ReceiverId)>;
  static void SetURLStoreCreationHookForTesting(URLStoreCreationHook* hook);

  base::WeakPtr<BlobUrlRegistry> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Optional fallback BlobUrlRegistry. If lookups for URLs in this registry
  // fail, they are retried in the fallback registry. This is used to allow
  // "child" storage partitions to resolve URLs created by their "parent", while
  // not allowing the reverse.
  base::WeakPtr<BlobUrlRegistry> fallback_;

  std::map<GURL, mojo::PendingRemote<blink::mojom::Blob>> url_to_blob_;
  // TODO(crbug.com/40775506): Remove this once experiment is over.
  std::map<GURL, base::UnguessableToken> url_to_unsafe_agent_cluster_id_;
  std::map<GURL, net::SchemefulSite> url_to_unsafe_top_level_site_;
  std::map<base::UnguessableToken,
           std::pair<GURL, mojo::PendingRemote<blink::mojom::Blob>>>
      token_to_url_and_blob_;

  std::map<GURL, blink::StorageKey> url_to_storage_key_;
  std::map<GURL, url::Origin> url_to_origin_;
  std::map<GURL, int> url_to_render_process_host_id_;

  // When the renderer uses the BlobUrlRegistry from a frame context or from a
  // main thread worklet context, a navigation-associated interface is used to
  // preserve message ordering. The receiver corresponding to that connection is
  // an AssociatedReceiver and gets stored in `frame_receivers_`. For workers
  // and threaded worklets, the receiver is a Receiver and gets stored in
  // `worker_receivers_`.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::BlobURLStore>
      frame_receivers_;
  mojo::UniqueReceiverSet<blink::mojom::BlobURLStore> worker_receivers_;

  base::WeakPtrFactory<BlobUrlRegistry> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_
