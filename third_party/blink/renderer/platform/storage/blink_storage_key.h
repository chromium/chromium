// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_

#include <iosfwd>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
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

 public:
  // Creates a BlinkStorageKey with a unique opaque origin and top-level site.
  BlinkStorageKey();

  // Creates a BlinkStorageKey with the given origin. `origin` must not be null.
  // `origin` can be opaque. This implicitly sets `top_level_site_` to the same
  // origin.
  // TODO(https://crbug.com/1271615): Remove or mark as test-only most of these
  // constructors and factory methods.
  explicit BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin);

  // Creates a BlinkStorageKey with the given origin, top-level site and nonce.
  // `origin` must not be null. `origin` can be opaque.
  // `nonce` can be null to create a key without a nonce.
  // `ancestor_chain_bit` must not be null, if it cannot be determined, default
  // to kSameSite.
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const BlinkSchemefulSite& top_level_site,
                  const base::UnguessableToken* nonce,
                  mojom::blink::AncestorChainBit ancestor_chain_bit);

  // Creates a BlinkStorageKey converting the given StorageKey `storage_key`.
  // NOLINTNEXTLINE(google-explicit-constructor)
  BlinkStorageKey(const StorageKey& storage_key);

  // Converts this BlinkStorageKey into a StorageKey.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator StorageKey() const;

  ~BlinkStorageKey() = default;

  BlinkStorageKey(const BlinkStorageKey& other) = default;
  BlinkStorageKey& operator=(const BlinkStorageKey& other) = default;
  BlinkStorageKey(BlinkStorageKey&& other) = default;
  BlinkStorageKey& operator=(BlinkStorageKey&& other) = default;

  static BlinkStorageKey CreateWithNonce(
      scoped_refptr<const SecurityOrigin> origin,
      const base::UnguessableToken& nonce);

  static BlinkStorageKey CreateFromStringForTesting(const WTF::String& origin);

  // Takes in a SecurityOrigin `origin` and a BlinkSchemefulSite
  // `top_level_site` and returns a BlinkStorageKey with a nullptr nonce and an
  // AncestorChainBit set based on whether `origin` and `top_level_site` are
  // schemeful-same-site. NOTE: The approach used by this method for calculating
  // the AncestorChainBit is different than what's done in production code,
  // where the whole frame tree is used. In other words, this method cannot be
  // used to create a StorageKey corresponding to a first-party iframe with a
  // cross-site ancestor (e.g., "a.com" -> "b.com" -> "a.com"). To create a
  // BlinkStorageKey for that scenario, use the BlinkStorageKey constructor that
  // has an AncestorChainBit parameter.
  static BlinkStorageKey CreateForTesting(
      scoped_refptr<const SecurityOrigin> origin,
      const BlinkSchemefulSite& top_level_site);

  // Tries to construct an instance from (potentially
  // untrusted) values that got received over Mojo.
  //
  // Returns whether successful or not. Doesn't touch
  // `out` if false is returned.  This returning true does
  // not mean that whoever sent the values did not lie,
  // merely that they are well-formed.
  //
  // This function should only be used for serializing from Mojo or
  // testing.
  static bool FromWire(
      scoped_refptr<const SecurityOrigin> origin,
      const BlinkSchemefulSite& top_level_site,
      const BlinkSchemefulSite& top_level_site_if_third_party_enabled,
      const absl::optional<base::UnguessableToken>& nonce,
      mojom::blink::AncestorChainBit ancestor_chain_bit,
      mojom::blink::AncestorChainBit ancestor_chain_bit_if_third_party_enabled,
      BlinkStorageKey& out);

  const scoped_refptr<const SecurityOrigin>& GetSecurityOrigin() const {
    return origin_;
  }

  const BlinkSchemefulSite& GetTopLevelSite() const { return top_level_site_; }

  const absl::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  mojom::blink::AncestorChainBit GetAncestorChainBit() const {
    return ancestor_chain_bit_;
  }

  String ToDebugString() const;

  // Returns a copy of what this storage key would have been if
  // `kThirdPartyStoragePartitioning` were enabled. This is a convenience
  // function for callsites that benefit from future functionality.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  BlinkStorageKey CopyWithForceEnabledThirdPartyStoragePartitioning() const {
    BlinkStorageKey storage_key = *this;
    storage_key.top_level_site_ =
        storage_key.top_level_site_if_third_party_enabled_;
    storage_key.ancestor_chain_bit_ =
        storage_key.ancestor_chain_bit_if_third_party_enabled_;
    return storage_key;
  }

  // Checks if every single member in a BlinkStorageKey matches those in
  // `other`. Since the *_if_third_party_enabled_ fields aren't used normally
  // this function is only useful for testing purposes. This function can be
  // removed when  the *_if_third_party_enabled_ fields are removed.
  bool ExactMatchForTesting(const blink::BlinkStorageKey& other) const;

 private:
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const base::UnguessableToken* nonce);

  scoped_refptr<const SecurityOrigin> origin_;
  BlinkSchemefulSite top_level_site_;
  // Stores the value `top_level_site_` would have had if
  // `kThirdPartyStoragePartitioning` were enabled. This isn't used in
  // serialization or comparison.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  BlinkSchemefulSite top_level_site_if_third_party_enabled_ = top_level_site_;
  absl::optional<base::UnguessableToken> nonce_;
  mojom::blink::AncestorChainBit ancestor_chain_bit_{
      mojom::blink::AncestorChainBit::kSameSite};
  // Stores the value `ancestor_chain_bit_` would have had if
  // `kThirdPartyStoragePartitioning` were enabled. This isn't used in
  // serialization or comparison.
  // TODO(crbug.com/1159586): Remove when no longer needed.
  mojom::blink::AncestorChainBit ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_;
};

PLATFORM_EXPORT
bool operator==(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
bool operator!=(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
std::ostream& operator<<(std::ostream&, const BlinkStorageKey&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
