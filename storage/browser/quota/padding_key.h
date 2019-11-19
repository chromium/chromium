// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_PADDING_KEY_H_
#define STORAGE_BROWSER_QUOTA_PADDING_KEY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "crypto/symmetric_key.h"
#include "url/gurl.h"

namespace storage {

COMPONENT_EXPORT(STORAGE_BROWSER)
const crypto::SymmetricKey* GetDefaultPaddingKey();

// Returns a copy of the default key used to calculate padding sizes.
//
// The default padding key is a singleton object whose value is randomly
// generated the first time it is requested on every browser startup. In
// CacheStorage, when a cache does not have a padding key, it is assigned the
// current default key.
COMPONENT_EXPORT(STORAGE_BROWSER)
std::unique_ptr<crypto::SymmetricKey> CopyDefaultPaddingKey();

// Builds a key whose value is the given string.
//
// May return null if deserializing fails (e.g. if the raw key is the wrong
// size).
COMPONENT_EXPORT(STORAGE_BROWSER)
std::unique_ptr<crypto::SymmetricKey> DeserializePaddingKey(
    const std::string& raw_key);

// Gets the raw value of the default padding key.
//
// Each cache stores the raw value of the key that should be used when
// calculating its padding size.
COMPONENT_EXPORT(STORAGE_BROWSER)
std::string SerializeDefaultPaddingKey();

// Resets the default key to a random value.
//
// Simulating a key change across a browser restart lets us test that padding
// calculations are using the appropriate key.
COMPONENT_EXPORT(STORAGE_BROWSER)
void ResetPaddingKeyForTesting();

// Computes the padding size for a resource.
//
// For AppCache, which does not support storing metadata for a resource,
// |has_metadata| will always be false.
//
// For CacheStorage, the padding size of an entry depends on whether it contains
// metadata (a.k.a. "side data"). If metadata is added to the entry, the entry
// must be assigned a new padding size. Otherwise, the growth in the entry's
// size would leak the exact size of the added metadata.
COMPONENT_EXPORT(STORAGE_BROWSER)
int64_t ComputeResponsePadding(const std::string& response_url,
                               const crypto::SymmetricKey* padding_key,
                               bool has_metadata);

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_PADDING_KEY_H_
