// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RESOURCE_ID_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RESOURCE_ID_TRAITS_H_

#include <limits>

#include "components/viz/common/resources/resource_id.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

template <>
struct HashTraits<viz::ResourceId> : GenericHashTraits<viz::ResourceId> {
  static uint32_t GetHash(const viz::ResourceId& id) {
    return blink::GetHash(id.GetUnsafeValue());
  }
  static const bool kEmptyValueIsZero = false;
  static viz::ResourceId EmptyValue() {
    return viz::ResourceId(std::numeric_limits<uint32_t>::max());
  }
  static viz::ResourceId DeletedValue() {
    return viz::ResourceId(std::numeric_limits<uint32_t>::max() - 1);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RESOURCE_ID_TRAITS_H_
