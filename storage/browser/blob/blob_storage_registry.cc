// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/blob/blob_url_utils.h"
#include "url/gurl.h"

namespace storage {

BlobStorageRegistry::BlobStorageRegistry() = default;

BlobStorageRegistry::~BlobStorageRegistry() {
  // Note: We don't bother calling the construction complete callbacks, as we
  // are only being destructed at the end of the life of the browser process.
  // So it shouldn't matter.
}

BlobEntry* BlobStorageRegistry::CreateEntry(
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition) {
  DCHECK(blob_map_.find(uuid) == blob_map_.end());
  std::unique_ptr<BlobEntry> entry =
      std::make_unique<BlobEntry>(content_type, content_disposition);
  BlobEntry* entry_ptr = entry.get();
  blob_map_[uuid] = std::move(entry);
  return entry_ptr;
}

bool BlobStorageRegistry::DeleteEntry(const std::string& uuid) {
  return blob_map_.erase(uuid) == 1;
}

bool BlobStorageRegistry::HasEntry(const std::string& uuid) const {
  return blob_map_.find(uuid) != blob_map_.end();
}

BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) {
  auto found = blob_map_.find(uuid);
  if (found == blob_map_.end())
    return nullptr;
  return found->second.get();
}

const BlobEntry* BlobStorageRegistry::GetEntry(const std::string& uuid) const {
  return const_cast<BlobStorageRegistry*>(this)->GetEntry(uuid);
}

bool BlobStorageRegistry::CreateUrlMapping(
    const GURL& blob_url,
    mojo::PendingRemote<blink::mojom::Blob> blob) {
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  if (IsURLMapped(blob_url))
    return false;
  url_to_blob_[blob_url] = mojo::Remote<blink::mojom::Blob>(std::move(blob));
  return true;
}

bool BlobStorageRegistry::DeleteURLMapping(const GURL& blob_url) {
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  auto found = url_to_blob_.find(blob_url);
  if (found == url_to_blob_.end())
    return false;
  url_to_blob_.erase(found);
  return true;
}

bool BlobStorageRegistry::IsURLMapped(const GURL& blob_url) const {
  return url_to_blob_.find(blob_url) != url_to_blob_.end();
}

mojo::PendingRemote<blink::mojom::Blob> BlobStorageRegistry::GetBlobFromURL(
    const GURL& url) {
  auto found = url_to_blob_.find(BlobUrlUtils::ClearUrlFragment(url));
  if (found == url_to_blob_.end())
    return mojo::NullRemote();
  mojo::PendingRemote<blink::mojom::Blob> result;
  found->second->Clone(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void BlobStorageRegistry::AddTokenMapping(
    const base::UnguessableToken& token,
    const GURL& url,
    mojo::PendingRemote<blink::mojom::Blob> blob) {
  DCHECK(token_to_url_and_blob_.find(token) == token_to_url_and_blob_.end());
  token_to_url_and_blob_.emplace(
      token,
      std::make_pair(url, mojo::Remote<blink::mojom::Blob>(std::move(blob))));
}

void BlobStorageRegistry::RemoveTokenMapping(
    const base::UnguessableToken& token) {
  DCHECK(token_to_url_and_blob_.find(token) != token_to_url_and_blob_.end());
  token_to_url_and_blob_.erase(token);
}

bool BlobStorageRegistry::GetTokenMapping(
    const base::UnguessableToken& token,
    GURL* url,
    mojo::PendingRemote<blink::mojom::Blob>* blob) const {
  auto it = token_to_url_and_blob_.find(token);
  if (it == token_to_url_and_blob_.end())
    return false;
  *url = it->second.first;
  it->second.second->Clone(blob->InitWithNewPipeAndPassReceiver());
  return true;
}

}  // namespace storage
