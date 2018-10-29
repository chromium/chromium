// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_factory.h"

#include <memory>

#include "base/single_thread_task_runner.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    mojom::HostedRendererType type,
    const GetGpuFactoriesCB& get_gpu_factories_cb,
    media::mojom::InterfaceFactory* interface_factory)
    : get_gpu_factories_cb_(get_gpu_factories_cb),
      interface_factory_(interface_factory),
      hosted_renderer_type_(type) {
  DCHECK(interface_factory_);
}

MojoRendererFactory::~MojoRendererFactory() = default;

void MojoRendererFactory::SetGetTypeSpecificIdCB(
    const GetTypeSpecificIdCB& get_type_specific_id) {
  get_type_specific_id_ = get_type_specific_id;
}

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* video_renderer_sink,
    const RequestOverlayInfoCB& /* request_overlay_info_cb */,
    const gfx::ColorSpace& /* target_color_space */) {
  std::unique_ptr<VideoOverlayFactory> overlay_factory;

  // |get_gpu_factories_cb_| can be null in the HLS/MediaPlayerRenderer case,
  // when we do not need to create video overlays.
  if (get_gpu_factories_cb_) {
    overlay_factory =
        std::make_unique<VideoOverlayFactory>(get_gpu_factories_cb_.Run());
  }

  return std::unique_ptr<Renderer>(
      new MojoRenderer(media_task_runner, std::move(overlay_factory),
                       video_renderer_sink, GetRendererPtr()));
}

mojom::RendererPtr MojoRendererFactory::GetRendererPtr() {
  mojom::RendererPtr renderer_ptr;

  if (interface_factory_) {
    interface_factory_->CreateRenderer(hosted_renderer_type_,
                                       get_type_specific_id_.is_null()
                                           ? std::string()
                                           : get_type_specific_id_.Run(),
                                       mojo::MakeRequest(&renderer_ptr));
  } else {
    NOTREACHED();
  }

  return renderer_ptr;
}

}  // namespace media
