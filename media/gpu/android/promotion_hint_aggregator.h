// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_PROMOTION_HINT_AGGREGATOR_H_
#define MEDIA_GPU_ANDROID_PROMOTION_HINT_AGGREGATOR_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// Receive lots of promotion hints, and aggregate them into a single signal.  A
// promotion hint is feedback from the compositor about whether a quad could be
// promoted to an overlay, or whether the compositor would refuse to do so.
// For example, the compositor won't promote a quad that's rotated, since an
// overlay can't do that.
class MEDIA_GPU_EXPORT PromotionHintAggregator {
 public:
  struct Hint {
    Hint(const gfx::Rect& _screen_rect, bool _is_promotable)
        : screen_rect(_screen_rect), is_promotable(_is_promotable) {}
    gfx::Rect screen_rect;
    bool is_promotable = false;

    bool operator==(const Hint& other) const {
      return other.screen_rect == screen_rect &&
             other.is_promotable == is_promotable;
    }
  };

  // Pass the hint by value to permit thread-hopping callbacks.
  using NotifyPromotionHintCB = base::RepeatingCallback<void(Hint hint)>;

  virtual ~PromotionHintAggregator() = default;

  // Notify us that an image has / would be drawn with the given hint.
  virtual void NotifyPromotionHint(const Hint& hint) = 0;

  // Returns true if and only if it's probably okay to promote the video.
  virtual bool IsSafeToPromote() = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_PROMOTION_HINT_AGGREGATOR_H_
