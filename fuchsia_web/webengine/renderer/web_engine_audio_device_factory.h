// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

class WebEngineAudioDeviceFactory final : public blink::AudioDeviceFactory {
 public:
  WebEngineAudioDeviceFactory();
  ~WebEngineAudioDeviceFactory() override;

  // blink::AudioDeviceFactory overrides.
  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      const blink::LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) override;
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_AUDIO_DEVICE_FACTORY_H_
