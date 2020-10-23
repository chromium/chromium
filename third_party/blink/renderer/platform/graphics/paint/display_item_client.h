// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_CLIENT_H_

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class RasterEffectOutset : uint8_t {
  kNone,
  kHalfPixel,
  kWholePixel,
};

// The class for objects that can be associated with display items. A
// DisplayItemClient object should live at least longer than the document cycle
// in which its display items are created during painting. After the document
// cycle, a pointer/reference to DisplayItemClient should be no longer
// dereferenced unless we can make sure the client is still alive.
class PLATFORM_EXPORT DisplayItemClient {
 public:
  DisplayItemClient() {
#if DCHECK_IS_ON()
    OnCreate();
#endif
  }
  virtual ~DisplayItemClient() {
#if DCHECK_IS_ON()
    OnDestroy();
#endif
  }

#if DCHECK_IS_ON()
  // Tests if this DisplayItemClient object has been created and has not been
  // deleted yet.
  bool IsAlive() const;
  String SafeDebugName(bool known_to_be_safe = false) const;
#endif

  virtual String DebugName() const = 0;

  // Needed for paint chunk clients only. Returns the id of the DOM node
  // associated with this DisplayItemClient, or kInvalidDOMNodeId if there is no
  // associated DOM node or this DisplayItemClient is never used as a paint
  // chunk client.
  virtual DOMNodeId OwnerNodeId() const { return kInvalidDOMNodeId; }

  // The outset will be used to inflate visual rect after the visual rect is
  // mapped into the space of the composited layer, for any special raster
  // effects that might expand the rastered pixel area.
  virtual RasterEffectOutset VisualRectOutsetForRasterEffects() const {
    return RasterEffectOutset::kNone;
  }

  // The rect that needs to be invalidated partially for rasterization in this
  // client. It's in the same coordinate space as VisualRect().
  virtual IntRect PartialInvalidationVisualRect() const { return IntRect(); }

  // Called by PaintController::FinishCycle() for all clients after painting.
  virtual void ClearPartialInvalidationVisualRect() const {}

  // Indicates that the client will paint display items different from the ones
  // cached by PaintController. However, PaintController allows a client to
  // paint new display items that are not cached or to no longer paint some
  // cached display items without calling this method.
  // See PaintController::ClientCacheIsValid() for more details.
  void Invalidate(
      PaintInvalidationReason reason = PaintInvalidationReason::kFull) const {
    // If a full invalidation reason is already set, do not overwrite it with
    // a new reason.
    if (IsFullPaintInvalidationReason(GetPaintInvalidationReason()) &&
        // However, kUncacheable overwrites any other reason.
        reason != PaintInvalidationReason::kUncacheable)
      return;
    paint_invalidation_reason_ = reason;
  }

  PaintInvalidationReason GetPaintInvalidationReason() const {
    return paint_invalidation_reason_;
  }

  // A client is considered "just created" if its display items have never been
  // validated by any PaintController since it's created.
  bool IsJustCreated() const {
    return paint_invalidation_reason_ == PaintInvalidationReason::kJustCreated;
  }

  // Whether the client is cacheable. The uncacheable status is set when the
  // client produces any display items that skipped caching of any
  // PaintController.
  bool IsCacheable() const {
    return paint_invalidation_reason_ != PaintInvalidationReason::kUncacheable;
  }

  // True if the client's display items are cached in PaintControllers without
  // needing to update.
  bool IsValid() const {
    return paint_invalidation_reason_ == PaintInvalidationReason::kNone;
  }

  String ToString() const;

 private:
  friend class FakeDisplayItemClient;
  friend class ObjectPaintInvalidatorTest;
  friend class PaintController;

  void Validate() const {
    paint_invalidation_reason_ = PaintInvalidationReason::kNone;
  }

#if DCHECK_IS_ON()
  void OnCreate();
  void OnDestroy();
#endif

  mutable PaintInvalidationReason paint_invalidation_reason_ =
      PaintInvalidationReason::kJustCreated;

  DISALLOW_COPY_AND_ASSIGN(DisplayItemClient);
};

inline bool operator==(const DisplayItemClient& client1,
                       const DisplayItemClient& client2) {
  return &client1 == &client2;
}
inline bool operator!=(const DisplayItemClient& client1,
                       const DisplayItemClient& client2) {
  return &client1 != &client2;
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const DisplayItemClient*);
PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const DisplayItemClient&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DISPLAY_ITEM_CLIENT_H_
