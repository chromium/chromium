// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"

namespace content {

class FuchsiaAudioDeviceFactory : public blink::WebAudioDeviceFactory {
 public:
  FuchsiaAudioDeviceFactory();
  ~FuchsiaAudioDeviceFactory() final;

 protected:
  scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) final;

  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) final;

  scoped_refptr<media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) final;

  scoped_refptr<media::AudioCapturerSource> CreateAudioCapturerSource(
      const blink::LocalFrameToken& frame_token,
      const media::AudioSourceParameters& params) final;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
