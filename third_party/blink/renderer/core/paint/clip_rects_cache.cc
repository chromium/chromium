// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_rects_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

void ClipRectsCache::Entry::Trace(Visitor* visitor) const {
  visitor->Trace(root);
}

}  // namespace blink
