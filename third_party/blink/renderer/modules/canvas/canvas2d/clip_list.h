// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CLIP_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CLIP_LIST_H_

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

class SkPath;

namespace cc {
class PaintCanvas;
}

namespace blink {

class ClipList {
  DISALLOW_NEW();

 public:
  ClipList() = default;
  ClipList(const ClipList&);
  ~ClipList() = default;

  void ClipPath(const SkPath&, AntiAliasingMode, const SkMatrix&);
  void Playback(cc::PaintCanvas*) const;
  const SkPath& GetCurrentClipPath() const;

 private:
  struct ClipOp {
    SkPath path_;
    AntiAliasingMode anti_aliasing_mode_;

    ClipOp();
    ClipOp(const ClipOp&);
  };

  // Number of clip ops that can be stored in a ClipList without resorting to
  // dynamic allocation
  static const size_t kCInlineClipOpCapacity = 4;

  WTF::Vector<ClipOp, kCInlineClipOpCapacity> clip_list_;
  SkPath current_clip_path_;
};

}  // namespace blink

#endif
