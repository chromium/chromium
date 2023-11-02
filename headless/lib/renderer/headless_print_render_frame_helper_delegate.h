// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

namespace headless {

class HeadlessPrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  HeadlessPrintRenderFrameHelperDelegate();

  HeadlessPrintRenderFrameHelperDelegate(
      const HeadlessPrintRenderFrameHelperDelegate&) = delete;
  HeadlessPrintRenderFrameHelperDelegate& operator=(
      const HeadlessPrintRenderFrameHelperDelegate&) = delete;

  ~HeadlessPrintRenderFrameHelperDelegate() override;

 private:
  // printing::PrintRenderFrameHelper::Delegate:
  bool IsPrintPreviewEnabled() override;
  bool ShouldGenerateTaggedPDF() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;
  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
