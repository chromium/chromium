// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_WEBAUDIO_MEDIA_STREAM_AUDIO_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_WEBAUDIO_MEDIA_STREAM_AUDIO_SINK_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/audio_converter.h"
#include "media/base/reentrancy_checker.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace media {
class AudioBus;
class AudioConverter;
class AudioFifo;
class AudioParameters;
}  // namespace media

namespace blink {

class WebAudioSourceProviderClient;

// WebAudioMediaStreamAudioSink provides a bridge between classes:
//     MediaStreamAudioTrack ---> WebAudioSourceProvider
//
// WebAudioMediaStreamAudioSink works as a sink to the MediaStreamAudioTrack
// and stores the capture data to a FIFO. When the media stream is connected to
// WebAudio MediaStreamAudioSourceNode as a source provider,
// MediaStreamAudioSourceNode will periodically call provideInput() to get the
// data from the FIFO.
//
// Most calls are protected by a lock.
class MODULES_EXPORT WebAudioMediaStreamAudioSink
    : public WebAudioSourceProvider,
      public media::AudioConverter::InputCallback,
      public WebMediaStreamAudioSink {
 public:
  static const size_t kWebAudioRenderBufferSize;

  explicit WebAudioMediaStreamAudioSink(const WebMediaStreamTrack& track,
                                        int context_sample_rate);
  ~WebAudioMediaStreamAudioSink() override;

  // WebMediaStreamAudioSink implementation.
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;
  void OnReadyStateChanged(WebMediaStreamSource::ReadyState state) override;

  // WebAudioSourceProvider implementation.
  void SetClient(WebAudioSourceProviderClient* client) override;
  void ProvideInput(const WebVector<float*>& audio_data,
                    size_t number_of_frames) override;

  // Method to allow the unittests to inject its own sink parameters to avoid
  // query the hardware.
  // TODO(xians,tommi): Remove and instead offer a way to inject the sink
  // parameters so that the implementation doesn't rely on the global default
  // hardware config but instead gets the parameters directly from the sink
  // (WebAudio in this case). Ideally the unit test should be able to use that
  // same mechanism to inject the sink parameters for testing.
  void SetSinkParamsForTesting(const media::AudioParameters& sink_params);

 private:
  // media::AudioConverter::InputCallback implementation.
  // This function is triggered by the above ProvideInput() on the WebAudio
  // audio thread, so it has be called under the protection of |lock_|.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override;

  std::unique_ptr<media::AudioConverter> audio_converter_ GUARDED_BY(lock_);
  std::unique_ptr<media::AudioFifo> fifo_ GUARDED_BY(lock_);
  bool is_enabled_ GUARDED_BY(lock_);
  media::AudioParameters source_params_ GUARDED_BY(lock_);
  media::AudioParameters sink_params_ GUARDED_BY(lock_);

  // Protects the above variables.
  base::Lock lock_;

  // No lock protection needed since only accessed in WebVector version of
  // ProvideInput().
  std::unique_ptr<media::AudioBus> output_wrapper_;

  // The audio track that this source provider is connected to.
  // No lock protection needed since only accessed in constructor and
  // destructor.
  WebMediaStreamTrack track_;

  // Flag to tell if the track has been stopped or not.
  // No lock protection needed since only accessed in constructor, destructor
  // and OnReadyStateChanged().
  bool track_stopped_;

  // Used to assert that OnData() is only accessed by one thread at a time. We
  // can't use a thread checker since thread may change.
  REENTRANCY_CHECKER(capture_reentrancy_checker_);

  // Used to assert that ProvideInput() is not accessed concurrently.
  REENTRANCY_CHECKER(provide_input_reentrancy_checker_);

  // Used to assert that OnReadyStateChanged() is not accessed concurrently.
  REENTRANCY_CHECKER(ready_state_reentrancy_checker_);

  DISALLOW_COPY_AND_ASSIGN(WebAudioMediaStreamAudioSink);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_WEBAUDIO_MEDIA_STREAM_AUDIO_SINK_H_
