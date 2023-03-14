// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

#include <limits>

namespace blink {

UniqueObjectId NewUniqueObjectId() {
  static UniqueObjectId counter = 0;
  return ++counter;
}

static CompositorElementId CreateCompositorElementId(
    uint64_t blink_id,
    CompositorElementIdNamespace namespace_id) {
  DCHECK(blink_id);
  DCHECK_LT(blink_id, std::numeric_limits<uint64_t>::max() /
                          static_cast<unsigned>(
                              CompositorElementIdNamespace::kMaxRepresentable));
  // Shift to make room for namespace_id enum bits.
  uint64_t id = blink_id << kCompositorNamespaceBitCount;
  id += static_cast<uint64_t>(namespace_id);
  return CompositorElementId(id);
}

CompositorElementId PLATFORM_EXPORT CompositorElementIdFromUniqueObjectId(
    UniqueObjectId id,
    CompositorElementIdNamespace namespace_id) {
  DCHECK_LE(namespace_id, CompositorElementIdNamespace::kMax);
  return CreateCompositorElementId(id, namespace_id);
}

CompositorElementId PLATFORM_EXPORT
CompositorElementIdWithNamespace(CompositorElementId element_id,
                                 CompositorElementIdNamespace namespace_id) {
  DCHECK_LE(namespace_id, CompositorElementIdNamespace::kMax);
  uint64_t id = element_id.GetInternalValue();
  id &= ~((1 << kCompositorNamespaceBitCount) - 1);
  id |= static_cast<uint64_t>(namespace_id);
  return CompositorElementId(id);
}

CompositorElementId PLATFORM_EXPORT
CompositorElementIdFromDOMNodeId(DOMNodeId id) {
  DCHECK_GE(id, 0);
  return CreateCompositorElementId(id,
                                   CompositorElementIdNamespace::kDOMNodeId);
}

CompositorElementId PLATFORM_EXPORT
CompositorElementIdFromUniqueObjectId(UniqueObjectId id) {
  return CreateCompositorElementId(
      id, CompositorElementIdNamespace::kUniqueObjectId);
}

CompositorElementIdNamespace NamespaceFromCompositorElementId(
    CompositorElementId element_id) {
  return static_cast<CompositorElementIdNamespace>(
      element_id.GetInternalValue() %
      static_cast<uint64_t>(CompositorElementIdNamespace::kMaxRepresentable));
}

DOMNodeId DOMNodeIdFromCompositorElementId(CompositorElementId element_id) {
  DCHECK_EQ(NamespaceFromCompositorElementId(element_id),
            CompositorElementIdNamespace::kDOMNodeId);
  return static_cast<DOMNodeId>(element_id.GetInternalValue() >>
                                kCompositorNamespaceBitCount);
}

}  // namespace blink
