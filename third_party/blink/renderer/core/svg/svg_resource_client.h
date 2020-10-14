// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_CLIENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class LayoutSVGResourceContainer;
class QualifiedName;
class SVGFilterPrimitiveStandardAttributes;

typedef unsigned InvalidationModeMask;

class CORE_EXPORT SVGResourceClient : public GarbageCollectedMixin {
 public:
  virtual ~SVGResourceClient() = default;

  // When adding modes, make sure we don't overflow
  // |LayoutSVGResourceContainer::completed_invalidation_mask_|.
  enum InvalidationMode {
    kLayoutInvalidation = 1 << 0,
    kBoundariesInvalidation = 1 << 1,
    kPaintInvalidation = 1 << 2,
    kPaintPropertiesInvalidation = 1 << 3,
    kClipCacheInvalidation = 1 << 4,
    kFilterCacheInvalidation = 1 << 5,
  };
  virtual void ResourceContentChanged(InvalidationModeMask) = 0;
  virtual void ResourceElementChanged() = 0;
  virtual void ResourceDestroyed(LayoutSVGResourceContainer*) {}

  virtual void FilterPrimitiveChanged(
      SVGFilterPrimitiveStandardAttributes& primitive,
      const QualifiedName& attribute) {
    ResourceContentChanged(kPaintInvalidation | kFilterCacheInvalidation);
  }

 protected:
  SVGResourceClient() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_CLIENT_H_
