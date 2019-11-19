// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/test_mojo_media_client.h"

#include <memory>

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_stream_sink.h"
#include "media/audio/audio_thread_impl.h"
#include "media/base/cdm_factory.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/null_video_sink.h"
#include "media/base/renderer_factory.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/renderers/default_renderer_factory.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "media/cdm/cdm_proxy.h"  // nogncheck
#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm_proxy.h"  // nogncheck
#endif

namespace media {

TestMojoMediaClient::TestMojoMediaClient() = default;

TestMojoMediaClient::~TestMojoMediaClient() {
  DVLOG(1) << __func__;

  if (audio_manager_) {
    audio_manager_->Shutdown();
    audio_manager_.reset();
  }
}

void TestMojoMediaClient::Initialize(
    service_manager::Connector* /* connector */) {
  InitializeMediaLibrary();
  // TODO(dalecurtis): We should find a single owner per process for the audio
  // manager or make it a lazy instance.  It's not safe to call Get()/Create()
  // across multiple threads...
  AudioManager* audio_manager = AudioManager::Get();
  if (!audio_manager) {
    audio_manager_ = media::AudioManager::CreateForTesting(
        std::make_unique<AudioThreadImpl>());
    // Flush the message loop to ensure that the audio manager is initialized.
    base::RunLoop().RunUntilIdle();
  }
}

std::unique_ptr<Renderer> TestMojoMediaClient::CreateRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const std::string& /* audio_device_id */) {
  // If called the first time, do one time initialization.
  if (!decoder_factory_) {
    decoder_factory_.reset(new media::DefaultDecoderFactory(nullptr));
  }

  if (!renderer_factory_) {
    renderer_factory_ = std::make_unique<DefaultRendererFactory>(
        media_log, decoder_factory_.get(),
        DefaultRendererFactory::GetGpuFactoriesCB());
  }

  // We cannot share AudioOutputStreamSink or NullVideoSink among different
  // RendererImpls. Thus create one for each Renderer creation.
  auto audio_sink = base::MakeRefCounted<AudioOutputStreamSink>();
  auto video_sink = std::make_unique<NullVideoSink>(
      false, base::TimeDelta::FromSecondsD(1.0 / 60),
      NullVideoSink::NewFrameCB(), task_runner);
  auto* video_sink_ptr = video_sink.get();

  // Hold created sinks since DefaultRendererFactory only takes raw pointers to
  // the sinks. We are not cleaning up them even after a created Renderer is
  // destroyed. But this is fine since this class is only used for tests.
  audio_sinks_.push_back(audio_sink);
  video_sinks_.push_back(std::move(video_sink));

  return renderer_factory_->CreateRenderer(
      task_runner, task_runner, audio_sink.get(), video_sink_ptr,
      RequestOverlayInfoCB(), gfx::ColorSpace());
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<Renderer> TestMojoMediaClient::CreateCastRenderer(
    service_manager::mojom::InterfaceProvider* host_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const base::UnguessableToken& /* overlay_plane_id */) {
  return CreateRenderer(host_interfaces, task_runner, media_log, std::string());
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

std::unique_ptr<CdmFactory> TestMojoMediaClient::CreateCdmFactory(
    service_manager::mojom::InterfaceProvider* /* host_interfaces */) {
  DVLOG(1) << __func__;
  return std::make_unique<DefaultCdmFactory>();
}

#if BUILDFLAG(ENABLE_CDM_PROXY)
std::unique_ptr<CdmProxy> TestMojoMediaClient::CreateCdmProxy(
    const base::Token& cdm_guid) {
  DVLOG(1) << __func__ << ": cdm_guid = " << cdm_guid.ToString();
  if (cdm_guid == kClearKeyCdmGuid)
    return std::make_unique<ClearKeyCdmProxy>();

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

}  // namespace media
