// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/clip_list.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

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
  for (const ClipOp* it = clip_list_.begin(); it < clip_list_.end(); it++) {
    canvas->clipPath(it->path_, SkClipOp::kIntersect,
                     it->anti_aliasing_mode_ == kAntiAliased);
  }
}

SkPath ClipList::IntersectPathWithClip(const SkPath& path) const {
  SkPath total = path;
  for (const ClipOp* it = clip_list_.begin(); it < clip_list_.end(); it++) {
    Op(total, it->path_, SkPathOp::kIntersect_SkPathOp, &total);
  }
  return total;
}

ClipList::ClipOp::ClipOp() : anti_aliasing_mode_(kAntiAliased) {}

ClipList::ClipOp::ClipOp(const ClipOp&) = default;

ClipList::ClipOp& ClipList::ClipOp::operator=(const ClipOp&) = default;

}  // namespace blink
