// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_print_render_frame_helper_delegate.h"
#include "base/command_line.h"
#include "components/headless/command_handler/headless_command_switches.h"
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
  // This is not necessary for --print-to-pdf command handling, however
  // preserved for backward compatibility with clients that use headless_shell
  // PDF printing via CDP Page.printToPDF without relying on its
  // 'generateTaggedPDF' option.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      headless::switches::kDisablePDFTagging);
}

bool HeadlessPrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace headless
