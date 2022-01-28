// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// Fence is a collection of fencedframe-related APIs that are visible
// at `window.fence`, only inside fencedframes.
// TODO(crbug.com/1123606) actually populate this object with APIs.
class CORE_EXPORT Fence final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Fence() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCE_API_H_
