// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_registry.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "net/base/features.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "storage/browser/blob/blob_url_utils.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/gurl.h"

namespace storage {

namespace {

BlobUrlRegistry::URLStoreCreationHook* g_url_store_creation_hook = nullptr;

}

BlobUrlRegistry::BlobUrlRegistry(base::WeakPtr<BlobUrlRegistry> fallback)
    : fallback_(std::move(fallback)) {}

BlobUrlRegistry::~BlobUrlRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BlobUrlRegistry::AddReceiver(
    const blink::StorageKey& storage_key,
    const url::Origin& renderer_origin,
    int render_process_host_id,
    mojo::PendingAssociatedReceiver<blink::mojom::BlobURLStore> receiver,
    base::RepeatingCallback<
        void(const GURL&, std::optional<blink::mojom::PartitioningBlobURLInfo>)>
        partitioning_blob_url_closure,
    base::RepeatingCallback<bool()> storage_access_check_callback,
    bool partitioning_disabled_by_policy) {
  mojo::ReceiverId receiver_id = frame_receivers_.Add(
      std::make_unique<storage::BlobURLStoreImpl>(
          storage_key, renderer_origin, render_process_host_id, AsWeakPtr(),
          storage::BlobURLValidityCheckBehavior::DEFAULT,
          std::move(partitioning_blob_url_closure),
          std::move(storage_access_check_callback),
          partitioning_disabled_by_policy),
      std::move(receiver));

  if (g_url_store_creation_hook) {
    g_url_store_creation_hook->Run(this, receiver_id);
  }
}

void BlobUrlRegistry::AddReceiver(
    const blink::StorageKey& storage_key,
    const url::Origin& renderer_origin,
    int render_process_host_id,
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver,
    base::RepeatingCallback<bool()> storage_access_check_callback,
    bool partitioning_disabled_by_policy,
    BlobURLValidityCheckBehavior validity_check_behavior) {
  worker_receivers_.Add(
      std::make_unique<storage::BlobURLStoreImpl>(
          storage_key, renderer_origin, render_process_host_id, AsWeakPtr(),
          validity_check_behavior, base::DoNothing(),
          std::move(storage_access_check_callback),
          partitioning_disabled_by_policy),
      std::move(receiver));
}

bool BlobUrlRegistry::AddUrlMapping(
    const GURL& blob_url,
    mojo::PendingRemote<blink::mojom::Blob> blob,
    const blink::StorageKey& storage_key,
    const url::Origin& renderer_origin,
    int render_process_host_id,
    // TODO(crbug.com/40775506): Remove these once experiment is over.
    const base::UnguessableToken& unsafe_agent_cluster_id,
    const std::optional<net::SchemefulSite>& unsafe_top_level_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  if (IsUrlMapped(blob_url, storage_key) ==
      BlobUrlRegistry::MappingStatus::kIsMapped) {
    return false;
  }
  url_to_unsafe_agent_cluster_id_[blob_url] = unsafe_agent_cluster_id;
  if (unsafe_top_level_site)
    url_to_unsafe_top_level_site_[blob_url] = *unsafe_top_level_site;
  url_to_blob_[blob_url] = std::move(blob);
  url_to_storage_key_[blob_url] = storage_key;
  url_to_origin_[blob_url] = renderer_origin;
  url_to_render_process_host_id_[blob_url] = render_process_host_id;
  return true;
}

bool BlobUrlRegistry::RemoveUrlMapping(const GURL& blob_url,
                                       const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  auto blob_it = url_to_blob_.find(blob_url);
  if (blob_it == url_to_blob_.end()) {
    return false;
  }
  if (url_to_storage_key_.at(blob_url) != storage_key) {
    return false;
  }
  url_to_blob_.erase(blob_it);
  url_to_unsafe_agent_cluster_id_.erase(blob_url);
  url_to_unsafe_top_level_site_.erase(blob_url);
  url_to_storage_key_.erase(blob_url);
  url_to_origin_.erase(blob_url);
  url_to_render_process_host_id_.erase(blob_url);
  return true;
}

url::Origin BlobUrlRegistry::GetOriginForNavigation(
    const GURL& url,
    const url::Origin& precursor_origin,
    std::optional<int> target_render_process_host_id) {
  // Some Blob URLs have the origin embedded directly within the URL, which we
  // can get from calling url::Origin::Create() (which will extract the embedded
  // origin if it exists). Cases whether the origin is not embedded within the
  // URL (when the content part is "null") would result in an opaque origin.
  url::Origin url_origin = url::Origin::Create(url);
  if (!url_origin.opaque()) {
    return url_origin;
  }

  // The origin is not embedded within the Blob URL. Strip out the ref from the
  // URL (if it exists), and get the origin from our mapping. If
  // `target_render_process_host_id` is set, only return the origin if it was
  // registered by a process with the same ID. This keeps the legacy behavior
  // where the blob URL's origin mapping lives on the renderer process, so only
  // the renderer where the blob URL is created knows its origin, so navigations
  // to other processes can't use the mapped origin.
  GURL url_without_ref = url.GetWithoutRef();
  auto it = url_to_origin_.find(url_without_ref);
  if (it != url_to_origin_.end() &&
      (!target_render_process_host_id.has_value() ||
       url_to_render_process_host_id_[url_without_ref] ==
           target_render_process_host_id.value())) {
    return it->second;
  }

  return url::Origin::Resolve(url, precursor_origin);
}

BlobUrlRegistry::MappingStatus BlobUrlRegistry::IsUrlMapped(
    const GURL& blob_url,
    const blink::StorageKey& storage_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::Contains(url_to_blob_, blob_url) &&
      base::Contains(url_to_storage_key_, blob_url)) {
    const blink::StorageKey& blob_url_key = url_to_storage_key_.at(blob_url);
    if (blob_url_key == storage_key) {
      return BlobUrlRegistry::MappingStatus::kIsMapped;
    }
    if (blob_url_key.origin() == storage_key.origin()) {
      if (blob_url_key.IsFirstPartyContext()) {
        return BlobUrlRegistry::MappingStatus::
            kNotMappedCrossPartitionSameOriginAccessFirstPartyBlobURL;
      }
      return BlobUrlRegistry::MappingStatus::
          kNotMappedCrossPartitionSameOriginAccessThirdPartyBlobURL;
    }
    // A fallback_ check isn't needed because a given Blob URL will either be
    // registered in this BlobUrlRegistry or registered in the fallback
    // BlobUrlRegistry but not both.
    return BlobUrlRegistry::MappingStatus::kNotMappedOther;
  }
  if (fallback_) {
    return fallback_->IsUrlMapped(blob_url, storage_key);
  }
  return BlobUrlRegistry::MappingStatus::kNotMappedOther;
}

// TODO(crbug.com/40775506): Remove this once experiment is over.
std::optional<base::UnguessableToken> BlobUrlRegistry::GetUnsafeAgentClusterID(
    const GURL& blob_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = url_to_unsafe_agent_cluster_id_.find(blob_url);
  if (it != url_to_unsafe_agent_cluster_id_.end())
    return it->second;
  if (fallback_)
    return fallback_->GetUnsafeAgentClusterID(blob_url);
  return std::nullopt;
}

std::optional<net::SchemefulSite> BlobUrlRegistry::GetUnsafeTopLevelSite(
    const GURL& blob_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = url_to_unsafe_top_level_site_.find(blob_url);
  if (it != url_to_unsafe_top_level_site_.end())
    return it->second;
  if (fallback_)
    return fallback_->GetUnsafeTopLevelSite(blob_url);
  return std::nullopt;
}

mojo::PendingRemote<blink::mojom::Blob> BlobUrlRegistry::GetBlobFromUrl(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = url_to_blob_.find(BlobUrlUtils::ClearUrlFragment(url));
  if (it == url_to_blob_.end())
    return fallback_ ? fallback_->GetBlobFromUrl(url) : mojo::NullRemote();
  mojo::Remote<blink::mojom::Blob> blob(std::move(it->second));
  mojo::PendingRemote<blink::mojom::Blob> result;
  blob->Clone(result.InitWithNewPipeAndPassReceiver());
  it->second = blob.Unbind();
  return result;
}

void BlobUrlRegistry::AddTokenMapping(
    const base::UnguessableToken& token,
    const GURL& url,
    mojo::PendingRemote<blink::mojom::Blob> blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(token_to_url_and_blob_, token));
  token_to_url_and_blob_.emplace(token, std::make_pair(url, std::move(blob)));
}

void BlobUrlRegistry::RemoveTokenMapping(const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(token_to_url_and_blob_, token));
  token_to_url_and_blob_.erase(token);
}

bool BlobUrlRegistry::GetTokenMapping(
    const base::UnguessableToken& token,
    GURL* url,
    mojo::PendingRemote<blink::mojom::Blob>* blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = token_to_url_and_blob_.find(token);
  if (it == token_to_url_and_blob_.end())
    return false;
  *url = it->second.first;
  mojo::Remote<blink::mojom::Blob> source_blob(std::move(it->second.second));
  source_blob->Clone(blob->InitWithNewPipeAndPassReceiver());
  it->second.second = source_blob.Unbind();
  return true;
}

// static
void BlobUrlRegistry::SetURLStoreCreationHookForTesting(
    URLStoreCreationHook* hook) {
  g_url_store_creation_hook = hook;
}

}  // namespace storage
