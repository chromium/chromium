// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_PAINT_IMAGES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_PAINT_IMAGES_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Not to be deleted through a pointer to the base class.
class PaintImages : public Vector<Persistent<StyleImage>> {
 public:
  std::unique_ptr<PaintImages> Clone() const {
    return base::WrapUnique(new PaintImages(*this));
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_PAINT_IMAGES_H_
