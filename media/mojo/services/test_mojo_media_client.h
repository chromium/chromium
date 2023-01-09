// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class AudioManager;
class AudioRendererSink;
class DecoderFactory;
class RendererFactory;
class VideoRendererSink;

// Test MojoMediaClient for MediaService.
class TestMojoMediaClient final : public MojoMediaClient {
 public:
  TestMojoMediaClient();

  TestMojoMediaClient(const TestMojoMediaClient&) = delete;
  TestMojoMediaClient& operator=(const TestMojoMediaClient&) = delete;

  ~TestMojoMediaClient() final;

  // MojoMediaClient implementation.
  void Initialize() final;
  std::unique_ptr<Renderer> CreateRenderer(
      mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      const std::string& audio_device_id) final;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  std::unique_ptr<Renderer> CreateCastRenderer(
      mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      const base::UnguessableToken& overlay_plane_id) final;
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* /* frame_interfaces */) final;

 private:
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<DecoderFactory> decoder_factory_;
  std::unique_ptr<RendererFactory> renderer_factory_;
  std::vector<scoped_refptr<AudioRendererSink>> audio_sinks_;
  std::vector<std::unique_ptr<VideoRendererSink>> video_sinks_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_TEST_MOJO_MEDIA_CLIENT_H_
