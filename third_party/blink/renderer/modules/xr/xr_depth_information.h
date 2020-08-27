// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XRDepthInformation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DOMUint16Array* data() const;

  uint32_t width() const;

  uint32_t height() const;

  float getDepth(uint32_t col, uint32_t row) const;

 private:
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_INFORMATION_H_
