// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ID_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ID_HASH_TRAITS_H_

#include "device/vr/public/mojom/anchor_id.h"
#include "device/vr/public/mojom/hit_test_subscription_id.h"
#include "device/vr/public/mojom/layer_id.h"
#include "device/vr/public/mojom/plane_id.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

template <typename T>
struct XrIdHashTraits : GenericHashTraits<T> {
  STATIC_ONLY(XrIdHashTraits);
  static unsigned GetHash(T key) {
    return blink::HashInt(key.GetUnsafeValue());
  }

  static constexpr bool kEmptyValueIsZero = true;
  static T EmptyValue() { return T(); }

  static T DeletedValue() {
    return T(std::numeric_limits<uint64_t>::max() - 1);
  }
};

template <>
struct HashTraits<device::PlaneId> : XrIdHashTraits<device::PlaneId> {
  STATIC_ONLY(HashTraits);
};

template <>
struct HashTraits<device::AnchorId> : XrIdHashTraits<device::AnchorId> {
  STATIC_ONLY(HashTraits);
};

template <>
struct HashTraits<device::LayerId> : XrIdHashTraits<device::LayerId> {
  STATIC_ONLY(HashTraits);
};

template <>
struct HashTraits<device::HitTestSubscriptionId>
    : XrIdHashTraits<device::HitTestSubscriptionId> {
  STATIC_ONLY(HashTraits);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ID_HASH_TRAITS_H_
