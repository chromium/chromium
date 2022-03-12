// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_mojo_media_client.h"

#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm_factory.h"
#include "media/mojo/services/media_foundation_renderer_wrapper.h"
#include "media/mojo/services/mojo_cdm_helper.h"

namespace media {

MediaFoundationMojoMediaClient::MediaFoundationMojoMediaClient() {
  DVLOG_FUNC(1);
}

MediaFoundationMojoMediaClient::~MediaFoundationMojoMediaClient() {
  DVLOG_FUNC(1);
}

std::unique_ptr<Renderer>
MediaFoundationMojoMediaClient::CreateMediaFoundationRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
        client_extension_remote) {
  DVLOG_FUNC(1);
  return std::make_unique<MediaFoundationRendererWrapper>(
      std::move(task_runner), frame_interfaces, std::move(media_log_remote),
      std::move(renderer_extension_receiver),
      std::move(client_extension_remote));
}

std::unique_ptr<CdmFactory> MediaFoundationMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  DVLOG_FUNC(1);
  return std::make_unique<MediaFoundationCdmFactory>(
      std::make_unique<MojoCdmHelper>(frame_interfaces));
}

}  // namespace media
