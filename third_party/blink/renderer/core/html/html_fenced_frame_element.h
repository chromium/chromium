// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FENCED_FRAME_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FENCED_FRAME_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

// HTMLFencedFrameElement implements the <fencedframe> element, which hosts the
// main frame of a top-level browsing context in an isolated frame. This element
// is non-standard and is currently being developed in
// https://github.com/shivanigithub/fenced-frame. As a result, this element is
// not exposed by default, but can be enabled by one of the following:
// - Enabling the Fenced Frames about:flags entry
// - Passing --enable-features=FencedFrames
class CORE_EXPORT HTMLFencedFrameElement : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLFencedFrameElement(Document& document);
  ~HTMLFencedFrameElement() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FENCED_FRAME_ELEMENT_H_
