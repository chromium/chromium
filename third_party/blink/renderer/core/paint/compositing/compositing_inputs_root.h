// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_ROOT_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;

class CompositingInputsRoot {
  DISALLOW_NEW();

 public:
  PaintLayer* Get() const { return root_layer_; }

  void Update(PaintLayer* new_root_layer);
  void Clear() { root_layer_ = nullptr; }

 private:
  PaintLayer* root_layer_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_ROOT_H_
