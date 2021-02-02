// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_QUOTA_PADDING_KEY_H_
#define STORAGE_COMMON_QUOTA_PADDING_KEY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "crypto/symmetric_key.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "url/gurl.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace storage {

COMPONENT_EXPORT(STORAGE_COMMON)
const crypto::SymmetricKey* GetDefaultPaddingKey();

// Returns a copy of the default key used to calculate padding sizes.
//
// The default padding key is a singleton object whose value is randomly
// generated the first time it is requested on every browser startup. In
// CacheStorage, when a cache does not have a padding key, it is assigned the
// current default key.
COMPONENT_EXPORT(STORAGE_COMMON)
std::unique_ptr<crypto::SymmetricKey> CopyDefaultPaddingKey();

// Builds a key whose value is the given string.
//
// May return null if deserializing fails (e.g. if the raw key is the wrong
// size).
COMPONENT_EXPORT(STORAGE_COMMON)
std::unique_ptr<crypto::SymmetricKey> DeserializePaddingKey(
    const std::string& raw_key);

// Gets the raw value of the default padding key.
//
// Each cache stores the raw value of the key that should be used when
// calculating its padding size.
COMPONENT_EXPORT(STORAGE_COMMON)
std::string SerializeDefaultPaddingKey();

// Resets the default key to a random value.
//
// Simulating a key change across a browser restart lets us test that padding
// calculations are using the appropriate key.
COMPONENT_EXPORT(STORAGE_COMMON)
void ResetPaddingKeyForTesting();

// Utility method to determine if a given type of response should be padded.
COMPONENT_EXPORT(STORAGE_COMMON)
bool ShouldPadResponseType(network::mojom::FetchResponseType type);

// Compute a purely random padding size for a resource.  A random padding is
// preferred except in cases where a site could rapidly trigger a large number
// of padded values for the same resource; e.g. from http cache.
COMPONENT_EXPORT(STORAGE_COMMON)
int64_t ComputeRandomResponsePadding();

// Compute a stable padding value for a resource.  This should be used for
// cases where a site could trigger a large number of padding values to be
// generated for the same resource; e.g. http cache.  The |origin| is the
// origin of the context that loaded the resource.  Note, its important that the
// |response_time| be the time stored in the cache and not just the current
// time.  The |side_data_size| should only be passed if padding is being
// computed for a side data blob.
COMPONENT_EXPORT(STORAGE_COMMON)
int64_t ComputeStableResponsePadding(const url::Origin& origin,
                                     const std::string& response_url,
                                     const base::Time& response_time,
                                     const std::string& request_method,
                                     int64_t side_data_size = 0);

}  // namespace storage

#endif  // STORAGE_COMMON_QUOTA_PADDING_KEY_H_
