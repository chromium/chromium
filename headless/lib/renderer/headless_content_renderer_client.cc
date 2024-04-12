// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/renderer/headless_content_renderer_client.h"

#include <memory>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "headless/public/switches.h"
#include "media/base/video_codecs.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/renderer/print_render_frame_helper.h"
#include "headless/lib/renderer/headless_print_render_frame_helper_delegate.h"
#endif

namespace headless {

HeadlessContentRendererClient::HeadlessContentRendererClient() {
  const auto& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  if (command_line.HasSwitch(switches::kAllowVideoCodecs)) {
    video_codecs_allowlist_.emplace(
        base::ToLowerASCII(
            command_line.GetSwitchValueASCII(switches::kAllowVideoCodecs)),
        /*default_allow=*/false);
  }
}

HeadlessContentRendererClient::~HeadlessContentRendererClient() = default;

void HeadlessContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
#if BUILDFLAG(ENABLE_PRINTING)
  new printing::PrintRenderFrameHelper(
      render_frame, std::make_unique<HeadlessPrintRenderFrameHelperDelegate>());
#endif
}

bool HeadlessContentRendererClient::IsSupportedVideoType(
    const media::VideoType& type) {
  const bool allowed_by_flags =
      !video_codecs_allowlist_ ||
      video_codecs_allowlist_->IsAllowed(
          base::ToLowerASCII(GetCodecName(type.codec)));
  // Besides being _allowed_, the codec actually has to be _supported_.
  return allowed_by_flags && ContentRendererClient::IsSupportedVideoType(type);
}

}  // namespace headless
