// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_

#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_client.h"

namespace content {

// RenderMediaClient is purely plumbing to make content embedder customizations
// visible to the lower media layer.
class CONTENT_EXPORT RenderMediaClient : public media::MediaClient {
 public:
  // Initialize RenderMediaClient and SetMediaClient(). Note that the instance
  // is not exposed because no content code needs to directly access it.
  static void Initialize();

  // MediaClient implementation.
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems)
      final;
  bool IsKeySystemsUpdateNeeded() final;
  bool IsSupportedAudioType(const media::AudioType& type) final;
  bool IsSupportedVideoType(const media::VideoType& type) final;
  bool IsSupportedBitstreamAudioCodec(media::AudioCodec codec) final;
  base::Optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      media::AudioParameters audio_parameters) final;

 private:
  RenderMediaClient();
  ~RenderMediaClient() override;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
