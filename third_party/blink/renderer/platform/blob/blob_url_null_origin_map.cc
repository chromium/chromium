// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/blob_url_null_origin_map.h"

#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
ThreadSpecific<BlobURLNullOriginMap>& BlobURLNullOriginMap::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BlobURLNullOriginMap>, map,
                                  ());
  return map;
}

void BlobURLNullOriginMap::Add(const KURL& blob_url, SecurityOrigin* origin) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  DCHECK(!blob_url.HasFragmentIdentifier());
  DCHECK(origin->SerializesAsNull());
  blob_url_null_origin_map_.insert(blob_url.GetString(), origin);
  if (origin->IsOpaque())
    BlobURLOpaqueOriginNonceMap::GetInstance().Add(blob_url, origin);
}

void BlobURLNullOriginMap::Remove(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  BlobURLOpaqueOriginNonceMap::GetInstance().Remove(blob_url);
  blob_url_null_origin_map_.erase(blob_url.GetString());
}

SecurityOrigin* BlobURLNullOriginMap::Get(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  KURL blob_url_without_fragment = blob_url;
  blob_url_without_fragment.RemoveFragmentIdentifier();
  auto it =
      blob_url_null_origin_map_.find(blob_url_without_fragment.GetString());
  return it != blob_url_null_origin_map_.end() ? it->value.get() : nullptr;
}

BlobURLOpaqueOriginNonceMap& BlobURLOpaqueOriginNonceMap::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(BlobURLOpaqueOriginNonceMap, map, ());
  return map;
}

void BlobURLOpaqueOriginNonceMap::Add(const KURL& blob_url,
                                      SecurityOrigin* origin) {
  base::AutoLock lock(lock_);
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  DCHECK(!blob_url.HasFragmentIdentifier());
  DCHECK(origin->IsOpaque());
  DCHECK(origin->GetNonceForSerialization());
  auto result = blob_url_opaque_origin_nonce_map_.insert(
      blob_url.GetString(), *origin->GetNonceForSerialization());
  // The blob URL must be registered only once within the process.
  SECURITY_CHECK(result.is_new_entry);
}

void BlobURLOpaqueOriginNonceMap::Remove(const KURL& blob_url) {
  base::AutoLock lock(lock_);
  DCHECK(blob_url.ProtocolIs("blob"));
  blob_url_opaque_origin_nonce_map_.erase(blob_url.GetString());
}

base::UnguessableToken BlobURLOpaqueOriginNonceMap::Get(const KURL& blob_url) {
  base::AutoLock lock(lock_);
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  KURL blob_url_without_fragment = blob_url;
  blob_url_without_fragment.RemoveFragmentIdentifier();
  auto it = blob_url_opaque_origin_nonce_map_.find(
      blob_url_without_fragment.GetString());
  return it != blob_url_opaque_origin_nonce_map_.end()
             ? it->value
             : base::UnguessableToken();
}

}  // namespace blink
