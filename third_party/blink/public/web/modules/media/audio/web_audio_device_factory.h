// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_DEVICE_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_DEVICE_FACTORY_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/audio/audio_source_parameters.h"
#include "media/base/audio_latency.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/web_common.h"

namespace media {
class AudioRendererSink;
class SwitchableAudioRendererSink;
class AudioCapturerSource;
}  // namespace media

namespace blink {

// A factory for creating AudioRendererSinks and AudioCapturerSources. There is
// a global factory function that can be installed for the purposes of testing
// to provide specialized implementations.
// TODO(olka): rename it, probably split it into AudioRendererSinkFactory and
// AudioCapturerSourceFactory.
//
// TODO(https://crrev.com/787252): Add a 'Web' prefix to the class name.
class BLINK_MODULES_EXPORT WebAudioDeviceFactory {
 public:
  // Maps the source type to the audio latency it requires.
  static media::AudioLatency::LatencyType GetSourceLatencyType(
      WebAudioDeviceSourceType source);

  // Creates a sink for AudioRendererMixer. |frame_token| refers to the
  // RenderFrame containing the entity producing the audio. Note: These sinks do
  // not support the blocking GetOutputDeviceInfo() API and instead clients are
  // required to use the GetOutputDeviceInfoAsync() API. As such they are
  // configured with no authorization timeout value.
  static scoped_refptr<media::AudioRendererSink> NewAudioRendererMixerSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates an AudioRendererSink bound to an AudioOutputDevice.
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  // TODO(olka): merge it with NewRestartableOutputDevice() as soon as
  // AudioOutputDevice is fixed to be restartable.
  static scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates a SwitchableAudioRendererSink bound to an AudioOutputDevice
  // Basing on |source_type| and build configuration, audio played out through
  // the sink goes to AOD directly or can be mixed with other audio before that.
  static scoped_refptr<media::SwitchableAudioRendererSink>
  NewSwitchableAudioRendererSink(WebAudioDeviceSourceType source_type,
                                 const LocalFrameToken& frame_token,
                                 const media::AudioSinkParameters& params);

  // A helper to get device info in the absence of AudioOutputDevice.
  // Must be called on renderer thread only.
  static media::OutputDeviceInfo GetOutputDeviceInfo(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params);

  // Creates an AudioCapturerSource using the currently registered factory.
  // |frame_token| refers to the RenderFrame containing the entity
  // consuming the audio.
  static scoped_refptr<media::AudioCapturerSource> NewAudioCapturerSource(
      const LocalFrameToken& frame_token,
      const media::AudioSourceParameters& params);

 protected:
  WebAudioDeviceFactory();
  virtual ~WebAudioDeviceFactory();

  // You can derive from this class and specify an implementation for these
  // functions to provide alternate audio device implementations.
  // If the return value of either of these function is NULL, we fall back
  // on the default implementation.

  // Creates a final sink in the rendering pipeline, which represents the actual
  // output device. |auth_timeout| is the authorization timeout allowed for the
  // underlying AudioOutputDevice instance; a timeout of zero means no timeout.
  virtual scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) = 0;

  virtual scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) = 0;

  virtual scoped_refptr<media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      WebAudioDeviceSourceType source_type,
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params) = 0;

  virtual scoped_refptr<media::AudioCapturerSource> CreateAudioCapturerSource(
      const LocalFrameToken& frame_token,
      const media::AudioSourceParameters& params) = 0;

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default AudioRendererSinks.
  static WebAudioDeviceFactory* factory_;

  static scoped_refptr<media::AudioRendererSink> NewFinalAudioRendererSink(
      const LocalFrameToken& frame_token,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout);

  DISALLOW_COPY_AND_ASSIGN(WebAudioDeviceFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_AUDIO_WEB_AUDIO_DEVICE_FACTORY_H_
