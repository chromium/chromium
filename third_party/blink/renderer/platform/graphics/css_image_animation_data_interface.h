// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CSS_IMAGE_ANIMATION_DATA_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CSS_IMAGE_ANIMATION_DATA_INTERFACE_H_

#include "third_party/blink/renderer/platform/graphics/image_node_animation_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class ImageResourceContent;

struct ImageAnimationData {
  // When image-animation is 'normal', elements sharing the same-url image
  // animate in sync and therefore share the same PaintImage Id.
  // When image-animation is 'paused' or 'running', each element may have an
  // independent animation timeline. This map tracks the PaintImage Id and
  // animation sequence Id for each such element. The animation timeline is
  // managed by the animation controller in CC Layer.
  // The sequence Id is used to pass sync update information to
  // the animation controller.
  PaintImage::Id paint_id = PaintImage::kInvalidId;
  PaintImage::AnimationSyncSequence sync_sequence =
      PaintImage::AnimationSyncSequence::kShared;
};

// An interface for CSS Image Animation per-node state, keyed by
// ImageResourceContent.
class PLATFORM_EXPORT ElementImageAnimationData {
 public:
  virtual ~ElementImageAnimationData() = default;

  // Returns the entry for "image". Tri-state for the caller:
  // nullopt = no entry recorded yet; sync_sequence==kShared = shared timeline
  // (paint_id cached); sync_sequence==kOwn = element has its own timeline.
  virtual std::optional<ImageAnimationData> GetImageAnimationData(
      ImageResourceContent*) const = 0;

  // Inserts or updates the entry for "image".
  virtual void SetImageAnimationData(ImageResourceContent*,
                                     ImageAnimationData) = 0;

  // Removes the entry for "image", returning to the no-entry state.
  virtual void EraseImageAnimationData(ImageResourceContent*) = 0;

 protected:
  ElementImageAnimationData() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CSS_IMAGE_ANIMATION_DATA_INTERFACE_H_
