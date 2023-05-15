// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

// MediaFoundation-specific MojoMediaClient implementation for
// MediaFoundationService running in the "Media Foundation Service" utility
// process hosting MediaFoundationRenderer and MediaFoundationCdm.
class MediaFoundationMojoMediaClient final : public MojoMediaClient {
 public:
  MediaFoundationMojoMediaClient();

  MediaFoundationMojoMediaClient(const MediaFoundationMojoMediaClient&) =
      delete;
  MediaFoundationMojoMediaClient& operator=(
      const MediaFoundationMojoMediaClient&) = delete;

  ~MediaFoundationMojoMediaClient() override;

  // MojoMediaClient implementation.
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) override;

  // MojoMediaClient implementation.
  std::unique_ptr<Renderer> CreateMediaFoundationRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingRemote<mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) override;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) override;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
