// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/test_mojo_media_client.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/cdm_factory.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/null_video_sink.h"
#include "media/base/renderer_factory.h"
#include "media/cdm/default_cdm_factory.h"
#include "media/renderers/default_decoder_factory.h"
#include "media/renderers/renderer_impl_factory.h"

namespace media {

TestMojoMediaClient::TestMojoMediaClient() = default;

TestMojoMediaClient::~TestMojoMediaClient() {
  DVLOG(1) << __func__;

  if (audio_manager_) {
    audio_manager_->Shutdown();
    audio_manager_.reset();
  }
}

void TestMojoMediaClient::Initialize() {
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
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const std::string& /* audio_device_id */) {
  // If called the first time, do one time initialization.
  if (!decoder_factory_) {
    decoder_factory_ = std::make_unique<media::DefaultDecoderFactory>(nullptr);
  }

  media::MediaPlayerLoggingID player_id = media::GetNextMediaPlayerLoggingID();

  if (!renderer_factory_) {
#if BUILDFLAG(IS_ANDROID)
    renderer_factory_ = std::make_unique<RendererImplFactory>(
        media_log, decoder_factory_.get(),
        RendererImplFactory::GetGpuFactoriesCB(), player_id);
#else
    renderer_factory_ = std::make_unique<RendererImplFactory>(
        media_log, decoder_factory_.get(),
        RendererImplFactory::GetGpuFactoriesCB(), player_id, nullptr);
#endif
  }

  // We cannot share the NullAudioSink or NullVideoSink among different
  // RendererImpls. Thus create one for each Renderer creation.
  auto audio_sink = base::MakeRefCounted<NullAudioSink>(task_runner);
  auto video_sink = std::make_unique<NullVideoSink>(
      false, base::Seconds(1.0 / 60), NullVideoSink::NewFrameCB(), task_runner);
  auto* video_sink_ptr = video_sink.get();

  // Hold created sinks since RendererImplFactory only takes raw pointers to
  // the sinks. We are not cleaning up them even after a created Renderer is
  // destroyed. But this is fine since this class is only used for tests.
  audio_sinks_.push_back(audio_sink);
  video_sinks_.push_back(std::move(video_sink));

  return renderer_factory_->CreateRenderer(
      task_runner, task_runner, audio_sink.get(), video_sink_ptr,
      base::NullCallback(), gfx::ColorSpace());
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<Renderer> TestMojoMediaClient::CreateCastRenderer(
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const base::UnguessableToken& /* overlay_plane_id */) {
  return CreateRenderer(frame_interfaces, task_runner, media_log,
                        std::string());
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

std::unique_ptr<CdmFactory> TestMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* /* frame_interfaces */) {
  DVLOG(1) << __func__;
  return std::make_unique<DefaultCdmFactory>();
}

}  // namespace media
