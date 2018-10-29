// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_ELEMENT_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_ELEMENT_ID_H_

#include <unordered_set>

#include "cc/trees/element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

const int kCompositorNamespaceBitCount = 4;

enum class CompositorElementIdNamespace {
  kPrimary,
  kUniqueObjectId,
  kScroll,
  kStickyTranslation,
  // The following are SPv2-only.
  kEffectFilter,
  kEffectMask,
  kEffectClipPath,
  kVerticalScrollbar,
  kHorizontalScrollbar,
  kOverscrollElasticity,
  // A sentinel to indicate the maximum representable namespace id
  // (the maximum is one less than this value).
  kMaxRepresentableNamespaceId = 1 << kCompositorNamespaceBitCount
};

using CompositorElementId = cc::ElementId;
using ScrollbarId = uint64_t;
using UniqueObjectId = uint64_t;
using DOMNodeId = uint64_t;
using SyntheticEffectId = uint64_t;

// Call this to get a globally unique object id for a newly allocated object.
UniqueObjectId PLATFORM_EXPORT NewUniqueObjectId();

// Call this with an appropriate namespace if more than one CompositorElementId
// is required for the given UniqueObjectId.
CompositorElementId PLATFORM_EXPORT
    CompositorElementIdFromUniqueObjectId(UniqueObjectId,
                                          CompositorElementIdNamespace);
// ...and otherwise call this method if there is only one CompositorElementId
// required for the given UniqueObjectId.
CompositorElementId PLATFORM_EXPORT
    CompositorElementIdFromUniqueObjectId(UniqueObjectId);

// TODO(chrishtr): refactor ScrollState to remove this dependency.
CompositorElementId PLATFORM_EXPORT CompositorElementIdFromDOMNodeId(DOMNodeId);

CompositorElementIdNamespace PLATFORM_EXPORT
    NamespaceFromCompositorElementId(CompositorElementId);

using CompositorElementIdSet =
    std::unordered_set<cc::ElementId, cc::ElementIdHash>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITOR_ELEMENT_ID_H_
