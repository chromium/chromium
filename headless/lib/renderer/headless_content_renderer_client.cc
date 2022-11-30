// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include <memory>

#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/renderer/print_render_frame_helper.h"
#include "headless/lib/renderer/headless_print_render_frame_helper_delegate.h"
#endif

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() = default;

HeadlessContentRendererClient::~HeadlessContentRendererClient() = default;

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<HeadlessPrintRenderFrameHelperDelegate>());
#endif
}

}  // namespace headless
