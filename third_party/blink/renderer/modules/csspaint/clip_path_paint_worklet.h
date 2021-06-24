// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CLIP_PATH_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CLIP_PATH_PAINT_WORKLET_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/modules/csspaint/native_paint_worklet.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class FloatRect;
class Image;
class LocalFrame;
class Node;

class MODULES_EXPORT ClipPathPaintWorklet : public NativePaintWorklet {
 public:
  static ClipPathPaintWorklet* Create(LocalFrame& local_root);

  using PassKey = base::PassKey<ClipPathPaintWorklet>;
  explicit ClipPathPaintWorklet(PassKey, LocalFrame& local_root);
  ~ClipPathPaintWorklet() final;
  ClipPathPaintWorklet(const ClipPathPaintWorklet&) = delete;
  ClipPathPaintWorklet& operator=(const ClipPathPaintWorklet&) = delete;

  scoped_refptr<Image> Paint(float zoom,
                             const FloatRect& reference_box,
                             const Node&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CLIP_PATH_PAINT_WORKLET_H_
