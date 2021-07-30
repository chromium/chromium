// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_device_factory.h"

namespace blink {

// TODO(https://crbug.com/787252): Move this class out of the Blink API layer.
class BLINK_MODULES_EXPORT FuchsiaAudioDeviceFactory final
    : public WebAudioDeviceFactory {
 public:
  FuchsiaAudioDeviceFactory();
  ~FuchsiaAudioDeviceFactory() override;

 protected:
  scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) override;

  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override;

  scoped_refptr<media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override;

  scoped_refptr<media::AudioCapturerSource> CreateAudioCapturerSource(
      const LocalFrameToken& frame_token,
      const media::AudioSourceParameters& params) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
