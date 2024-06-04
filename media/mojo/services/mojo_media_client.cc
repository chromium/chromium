// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer.h"
#include "media/base/video_decoder.h"

namespace media {

MojoMediaClient::MojoMediaClient() = default;

MojoMediaClient::~MojoMediaClient() = default;

void MojoMediaClient::Initialize() {}

std::unique_ptr<AudioDecoder> MojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<media::MediaLog> media_log) {
  return nullptr;
}

std::unique_ptr<AudioEncoder> MojoMediaClient::CreateAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return nullptr;
}

SupportedAudioDecoderConfigs
MojoMediaClient::GetSupportedAudioDecoderConfigs() {
  return {};
}

SupportedVideoDecoderConfigs
MojoMediaClient::GetSupportedVideoDecoderConfigs() {
  return {};
}

VideoDecoderType MojoMediaClient::GetDecoderImplementationType() {
  return VideoDecoderType::kUnknown;
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void MojoMediaClient::NotifyDecoderSupportKnown(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    base::OnceCallback<
        void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
  std::move(cb).Run(std::move(oop_video_decoder));
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

std::unique_ptr<VideoDecoder> MojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder) {
  return nullptr;
}

std::unique_ptr<Renderer> MojoMediaClient::CreateRenderer(
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const std::string& audio_device_id) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<Renderer> MojoMediaClient::CreateCastRenderer(
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const base::UnguessableToken& overlay_plane_id) {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(IS_WIN)
std::unique_ptr<Renderer> MojoMediaClient::CreateMediaFoundationRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_WIN)

std::unique_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

}  // namespace media
