// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_CODE_CACHE_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_CODE_CACHE_UTIL_H_

#include <stddef.h>

namespace blink {

// Size of the CachedMetadataHeader struct (see
// third_party/blink/renderer/platform/loader/fetch/cached_metadata.h).
// This header is prefixed to all cached metadata blobs and contains information
// like the data's encoding type and padding.
inline constexpr size_t kCodeCacheCachedMetadataHeaderSize = 16;

// Size of the timestamp (typically a uint64_t) data stored in the code cache.
inline constexpr size_t kCodeCacheTimestampSize = 8;

// Total size of the cached metadata when only a timestamp is stored.
// This includes the kCodeCacheCachedMetadataHeaderSize and the
// kCodeCacheTimestampSize itself.
inline constexpr size_t kCodeCacheTimestampCachedMetaSize =
    kCodeCacheCachedMetadataHeaderSize + kCodeCacheTimestampSize;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_LOADER_CODE_CACHE_UTIL_H_
