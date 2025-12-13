// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CLIP_PATH_PAINT_IMAGE_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CLIP_PATH_PAINT_IMAGE_GENERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/native_paint_image_generator.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class Image;
class LocalFrame;
class Node;

class CORE_EXPORT ClipPathPaintImageGenerator
    : public NativePaintImageGenerator {
 public:
  static ClipPathPaintImageGenerator* Create(LocalFrame& local_root);

  ~ClipPathPaintImageGenerator() override = default;

  using ClipPathPaintImageGeneratorCreateFunction =
      ClipPathPaintImageGenerator*(LocalFrame&);
  static void Init(ClipPathPaintImageGeneratorCreateFunction* create_function);

  // Returns a rect that will contain every possible keyframe of clip path
  // animation on the given layout object. This function assumes that it has
  // already been determined that such an animation exists and has no known
  // reasons why it should be disqualified from running. In future, The function
  // will attempt to return a minimal rect (such that the difference between the
  // union of all keyframes and the rect is as small as possible). If this is
  // is not possible for some reason, it will return either nullopt, if the
  // animation cannot be contained, or InfiniteIntRect() if the animation
  // needs to be clipped by the cull rect during paint-time.
  virtual std::optional<gfx::RectF> GetAnimationBoundingRect(
      const LayoutObject& obj) = 0;

  virtual scoped_refptr<Image> Paint(float zoom,
                                     const gfx::RectF& reference_box,
                                     const gfx::RectF& clip_area_rect,
                                     const Node&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CLIP_PATH_PAINT_IMAGE_GENERATOR_H_
