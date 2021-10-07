// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_
#define FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"

class WebEngineAudioDeviceFactory final : public blink::WebAudioDeviceFactory {
 public:
  WebEngineAudioDeviceFactory();
  ~WebEngineAudioDeviceFactory() override;

 protected:
  scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) override;

  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override;

  scoped_refptr<media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override;

  scoped_refptr<media::AudioCapturerSource> CreateAudioCapturerSource(
      const blink::LocalFrameToken& frame_token,
      const media::AudioSourceParameters& params) override;
};

#endif  // FUCHSIA_ENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_
