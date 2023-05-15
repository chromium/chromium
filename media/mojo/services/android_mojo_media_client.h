// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class AndroidMojoMediaClient final : public MojoMediaClient {
 public:
  AndroidMojoMediaClient();

  AndroidMojoMediaClient(const AndroidMojoMediaClient&) = delete;
  AndroidMojoMediaClient& operator=(const AndroidMojoMediaClient&) = delete;

  ~AndroidMojoMediaClient() override;

  // MojoMediaClient implementation.
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) override;

  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) override;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_ANDROID_MOJO_MEDIA_CLIENT_H_
