// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/courier_renderer_factory.h"

#include <memory>

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "media/base/overlay_info.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
#include "media/remoting/courier_renderer.h"  // nogncheck
#endif

namespace media {
namespace remoting {

CourierRendererFactory::CourierRendererFactory(
    std::unique_ptr<RendererController> controller)
    : controller_(std::move(controller)) {}

CourierRendererFactory::~CourierRendererFactory() = default;

std::unique_ptr<Renderer> CourierRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(IsRemotingActive());
#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  return std::make_unique<CourierRenderer>(
      media_task_runner, controller_->GetWeakPtr(), video_renderer_sink);
#else
  return nullptr;
#endif
}

bool CourierRendererFactory::IsRemotingActive() {
#if BUILDFLAG(IS_ANDROID)
  return false;  // Media Remoting is not supported on Android for now.
#else
  return controller_ && controller_->remote_rendering_started();
#endif
}

}  // namespace remoting
}  // namespace media
