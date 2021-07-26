// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_

#include <iosfwd>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This class represents the key by which DOM Storage keys its
// CachedStorageAreas.
// It is typemapped to blink.mojom.StorageKey, and should stay in sync with
// blink::StorageKey (third_party/blink/public/common/storage_key/storage_key.h)
class PLATFORM_EXPORT BlinkStorageKey {
  DISALLOW_NEW();

 public:
  // Creates a BlinkStorageKey with a unique opaque origin.
  BlinkStorageKey();

  // Creates a BlinkStorageKey with the given origin. `origin` must not be null.
  // `origin` can be opaque.
  explicit BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin);

  // Creates a BlinkStorageKey converting the given StorageKey `storage_key`.
  BlinkStorageKey(const StorageKey& storage_key);

  // Converts this BlinkStorageKey into a StorageKey.
  operator StorageKey() const;

  ~BlinkStorageKey() = default;

  BlinkStorageKey(const BlinkStorageKey& other) = default;
  BlinkStorageKey& operator=(const BlinkStorageKey& other) = default;
  BlinkStorageKey(BlinkStorageKey&& other) = default;
  BlinkStorageKey& operator=(BlinkStorageKey&& other) = default;

  static BlinkStorageKey CreateWithNonce(
      scoped_refptr<const SecurityOrigin> origin,
      const base::UnguessableToken& nonce);

  const scoped_refptr<const SecurityOrigin>& GetSecurityOrigin() const {
    return origin_;
  }

  const absl::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  String ToDebugString() const;

 private:
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const base::UnguessableToken* nonce);

  scoped_refptr<const SecurityOrigin> origin_;
  absl::optional<base::UnguessableToken> nonce_;
};

PLATFORM_EXPORT
bool operator==(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
bool operator!=(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
std::ostream& operator<<(std::ostream&, const BlinkStorageKey&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
