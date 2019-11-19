// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MEDIA_MULTI_CHANNEL_RESAMPLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MEDIA_MULTI_CHANNEL_RESAMPLER_H_

#include <memory>
#include "base/callback.h"
#include "base/macros.h"
#include "media/base/multi_channel_resampler.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace media {
class AudioBus;
}  // namespace media

namespace blink {

class AudioBus;

// This is a simple wrapper around the MultiChannelResampler provided by the
// media layer.
class PLATFORM_EXPORT MediaMultiChannelResampler {
  USING_FAST_MALLOC(MediaMultiChannelResampler);

  // Callback type for providing more data into the resampler.  Expects AudioBus
  // to be completely filled with data upon return; zero padded if not enough
  // frames are available to satisfy the request.  |frame_delay| is the number
  // of output frames already processed and can be used to estimate delay.
  typedef WTF::CrossThreadRepeatingFunction<void(int frame_delay,
                                                 AudioBus* audio_bus)>
      ReadCB;

 public:
  // Constructs a MultiChannelResampler with the specified |read_cb|, which is
  // used to acquire audio data for resampling.  |io_sample_rate_ratio| is the
  // ratio of input / output sample rates.  |request_frames| is the size in
  // frames of the AudioBus to be filled by |read_cb|.
  MediaMultiChannelResampler(int channels,
                             double io_sample_rate_ratio,
                             size_t request_frames,
                             ReadCB read_cb);

  // Resamples |frames| of data from |read_cb_| into AudioBus.
  void Resample(int frames, media::AudioBus* audio_bus);

 private:
  // Wrapper method used to provide input to the media::MultiChannelResampler
  // with a media::AudioBus rather than a blink::AudioBus.
  void ProvideResamplerInput(int resampler_frame_delay, media::AudioBus* dest);

  // The resampler being wrapped by this class.
  std::unique_ptr<media::MultiChannelResampler> resampler_;

  // The callback using a blink::AudioBus that will be called by
  // ProvideResamplerInput().
  ReadCB read_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaMultiChannelResampler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_MEDIA_MULTI_CHANNEL_RESAMPLER_H_
