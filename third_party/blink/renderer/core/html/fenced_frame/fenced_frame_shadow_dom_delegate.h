// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_SHADOW_DOM_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_SHADOW_DOM_DELEGATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

namespace blink {

class KURL;

// This is one of the underlying implementations of the `HTMLFencedFrameElement`
// interface. It can be activated by enabling the
// `blink::features::kFencedFrames` feature, and setting its feature param value
// to `FencedFramesImplementationType::kShadowDOM`. See the documentation above
// `FencedFrameDelegate`.
class CORE_EXPORT FencedFrameShadowDOMDelegate
    : public HTMLFencedFrameElement::FencedFrameDelegate {
 public:
  explicit FencedFrameShadowDOMDelegate(HTMLFencedFrameElement* outer_element);

  void DidGetInserted() override;
  void Navigate(const KURL&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_SHADOW_DOM_DELEGATE_H_
