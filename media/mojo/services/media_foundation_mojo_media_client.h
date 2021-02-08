// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

// This class is the |mojo_media_client| parameter to create
// media::MediaService. The MediaService itself is running in the mf_cdm utility
// process to host MediaFoundationRenderer/Cdm.
class MediaFoundationMojoMediaClient : public media::MojoMediaClient {
 public:
  MediaFoundationMojoMediaClient();
  ~MediaFoundationMojoMediaClient() final;

  // MojoMediaClient implementation.
  std::unique_ptr<media::CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaFoundationMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_MOJO_MEDIA_CLIENT_H_
