// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer.h"
#include "media/base/video_decoder.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/cdm/cdm_proxy.h"
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

namespace media {

MojoMediaClient::MojoMediaClient() = default;

MojoMediaClient::~MojoMediaClient() = default;

void MojoMediaClient::Initialize(service_manager::Connector* connector) {}

std::unique_ptr<AudioDecoder> MojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

SupportedVideoDecoderConfigMap
MojoMediaClient::GetSupportedVideoDecoderConfigs() {
  return {};
}

std::unique_ptr<VideoDecoder> MojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    VideoDecoderImplementation implementation,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  return nullptr;
}

std::unique_ptr<Renderer> MojoMediaClient::CreateRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const std::string& audio_device_id) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<Renderer> MojoMediaClient::CreateCastRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const base::UnguessableToken& overlay_plane_id) {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

std::unique_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* host_interfaces) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
std::unique_ptr<CdmProxy> MojoMediaClient::CreateCdmProxy(
    const base::Token& cdm_guid) {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

}  // namespace media
