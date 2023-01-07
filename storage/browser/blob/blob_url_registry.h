// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_

#include <map>

#include "base/component_export.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

class GURL;

namespace storage {

// This class stores the mapping of blob Urls to blobs.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobUrlRegistry {
 public:
  explicit BlobUrlRegistry(base::WeakPtr<BlobUrlRegistry> fallback = nullptr);

  BlobUrlRegistry(const BlobUrlRegistry&) = delete;
  BlobUrlRegistry& operator=(const BlobUrlRegistry&) = delete;

  ~BlobUrlRegistry();

  // Creates a url mapping from blob to the given url. Returns false if
  // there already is a map for the URL.
  bool AddUrlMapping(
      const GURL& url,
      mojo::PendingRemote<blink::mojom::Blob> blob,
      // TODO(https://crbug.com/1224926): Remove these once experiment is over.
      const base::UnguessableToken& unsafe_agent_cluster_id,
      const absl::optional<net::SchemefulSite>& unsafe_top_level_site);

  // Removes the given URL mapping. Returns false if the url wasn't mapped.
  bool RemoveUrlMapping(const GURL& url);

  // Returns if the url is mapped to a blob.
  bool IsUrlMapped(const GURL& blob_url) const;

  // TODO(https://crbug.com/1224926): Remove this once experiment is over.
  absl::optional<base::UnguessableToken> GetUnsafeAgentClusterID(
      const GURL& blob_url) const;
  absl::optional<net::SchemefulSite> GetUnsafeTopLevelSite(
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
  // TODO(https://crbug.com/1224926): Remove this once experiment is over.
  std::map<GURL, base::UnguessableToken> url_to_unsafe_agent_cluster_id_;
  std::map<GURL, net::SchemefulSite> url_to_unsafe_top_level_site_;
  std::map<base::UnguessableToken,
           std::pair<GURL, mojo::PendingRemote<blink::mojom::Blob>>>
      token_to_url_and_blob_;

  base::WeakPtrFactory<BlobUrlRegistry> weak_ptr_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REGISTRY_H_
