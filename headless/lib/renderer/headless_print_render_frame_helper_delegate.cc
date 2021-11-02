// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_print_render_frame_helper_delegate.h"
#include "third_party/blink/public/web/web_element.h"

namespace headless {

HeadlessPrintRenderFrameHelperDelegate::
    HeadlessPrintRenderFrameHelperDelegate() = default;

HeadlessPrintRenderFrameHelperDelegate::
    ~HeadlessPrintRenderFrameHelperDelegate() = default;

blink::WebElement HeadlessPrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool HeadlessPrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  return false;
}

bool HeadlessPrintRenderFrameHelperDelegate::ShouldGenerateTaggedPDF() {
  // Always generate tagged PDF, see: https://crbug.com/607777
  return true;
}

bool HeadlessPrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace headless
