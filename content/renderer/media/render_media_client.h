// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_

#include "base/synchronization/waitable_event.h"
#include "media/base/audio_parameters.h"
#include "media/base/decoder.h"
#include "media/base/media_client.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace content {

// RenderMediaClient is purely plumbing to make content embedder customizations
// visible to the lower media layer.
class RenderMediaClient : public media::MediaClient {
 public:
  RenderMediaClient(const RenderMediaClient&) = delete;
  RenderMediaClient& operator=(const RenderMediaClient&) = delete;

  // Initialize RenderMediaClient and SetMediaClient(). Note that the instance
  // is not exposed because no content code needs to directly access it.
  static void Initialize();

  // MediaClient implementation.
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) final;
  bool IsSupportedAudioType(const media::AudioType& type) final;
  bool IsSupportedVideoType(const media::VideoType& type) final;
  bool IsSupportedBitstreamAudioCodec(media::AudioCodec codec) final;
  absl::optional<::media::AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      media::AudioParameters audio_parameters) final;

 private:
  RenderMediaClient();
  ~RenderMediaClient() override;

  void OnGetSupportedVideoDecoderConfigs(
      const media::SupportedVideoDecoderConfigs& configs,
      media::VideoDecoderType type);

  void ResetConnectionForSupportedProfilesQueryOnMainThread()
      VALID_CONTEXT_REQUIRED(main_thread_sequence_checker_);

  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  SEQUENCE_CHECKER(main_thread_sequence_checker_);

  // Used to indicate if optional video profile support information has been
  // retrieved from the MojoVideoDecoder. May be waited upon by any thread but
  // the RenderThread since it's always signaled from the RenderThread.
  base::WaitableEvent did_update_;

  [[maybe_unused]] mojo::Remote<media::mojom::InterfaceFactory>
      interface_factory_for_supported_profiles_
          GUARDED_BY_CONTEXT(main_thread_sequence_checker_);
  [[maybe_unused]] mojo::SharedRemote<media::mojom::VideoDecoder>
      video_decoder_for_supported_profiles_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_CLIENT_H_
