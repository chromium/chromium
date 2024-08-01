// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/canvas/canvas2d/clip_list.h"

#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

class SkMatrix;

namespace blink {

ClipList::ClipList(const ClipList& other) = default;

void ClipList::ClipPath(const SkPath& path,
                        AntiAliasingMode anti_aliasing_mode,
                        const SkMatrix& ctm) {
  ClipOp new_clip;
  new_clip.anti_aliasing_mode_ = anti_aliasing_mode;
  new_clip.path_ = path;
  new_clip.path_.transform(ctm);
  clip_list_.push_back(new_clip);
}

void ClipList::Playback(cc::PaintCanvas* canvas) const {
  for (const auto& clip : clip_list_) {
    canvas->clipPath(clip.path_, SkClipOp::kIntersect,
                     clip.anti_aliasing_mode_ == kAntiAliased);
  }
}

SkPath ClipList::IntersectPathWithClip(const SkPath& path) const {
  SkPath total = path;
  for (const auto& clip : clip_list_) {
    Op(total, clip.path_, SkPathOp::kIntersect_SkPathOp, &total);
  }
  return total;
}

ClipList::ClipOp::ClipOp() : anti_aliasing_mode_(kAntiAliased) {}

ClipList::ClipOp::ClipOp(const ClipOp&) = default;

ClipList::ClipOp& ClipList::ClipOp::operator=(const ClipOp&) = default;

}  // namespace blink
