// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioConverter is a complete mixing, resampling, buffering, and channel
// mixing solution for converting data from one set of AudioParameters to
// another.
//
// For efficiency, pieces are only invoked when necessary; i.e.,
//    - The resampler is only used if sample rates differ.
//    - The FIFO is only used if buffer sizes differ.
//    - The channel mixer is only used if channel layouts differ.
//
// Additionally, since resampling is the most expensive operation, input mixing
// and channel down mixing are done prior to resampling.  Likewise, channel up
// mixing is performed after resampling.

#ifndef MEDIA_BASE_AUDIO_CONVERTER_H_
#define MEDIA_BASE_AUDIO_CONVERTER_H_

#include <list>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;
class AudioPullFifo;
class ChannelMixer;
class MultiChannelResampler;

// Converts audio data between two AudioParameters formats.  Sample usage:
//   AudioParameters input(...), output(...);
//   AudioConverter ac(input, output);
//   std::unique_ptr<AudioBus> output_audio_bus = AudioBus::Create(output);
//   ac.AddInput(<AudioConverter::InputCallback* 1>);
//   ac.AddInput(<AudioConverter::InputCallback* 2>);
//   ac.Convert(output_audio_bus.get());
//
// Convert() will ask for input audio data from each InputCallback and convert
// the data into the provided AudioBus.
class MEDIA_EXPORT AudioConverter {
 public:
  // Interface for inputs into the converter.  Each InputCallback is added or
  // removed from Convert() processing via AddInput() and RemoveInput().
  class MEDIA_EXPORT InputCallback {
   public:
    // Method for providing more data into the converter.  Expects |audio_bus|
    // to be completely filled with data upon return; zero padded if not enough
    // frames are available to satisfy the request.  The return value is the
    // volume level of the provided audio data.  If a volume level of zero is
    // returned no further processing will be done on the provided data, else
    // the volume level will be used to scale the provided audio data.
    // |frames_delayed| is given in terms of the input sample rate.
    virtual double ProvideInput(AudioBus* audio_bus,
                                uint32_t frames_delayed,
                                const AudioGlitchInfo& glitch_info) = 0;

   protected:
    virtual ~InputCallback() {}
  };

  // Constructs an AudioConverter for converting between the given input and
  // output parameters.  Specifying |disable_fifo| means all InputCallbacks are
  // capable of handling arbitrary buffer size requests; i.e. one call might ask
  // for 10 frames of data (indicated by the size of AudioBus provided) and the
  // next might ask for 20.  In synthetic testing, disabling the FIFO yields a
  // ~20% speed up for common cases.
  AudioConverter(const AudioParameters& input_params,
                 const AudioParameters& output_params,
                 bool disable_fifo);

  AudioConverter(const AudioConverter&) = delete;
  AudioConverter& operator=(const AudioConverter&) = delete;

  ~AudioConverter();

  // Converts audio from all inputs into the |dest|. If |frames_delayed| is
  // specified, it will be propagated to each input. Count of frames must be
  // given in terms of the output sample rate. If |glitch_info| is specified, it
  // will be accumulated and propagated to all inputs on the next call to
  // InputCallback::ProvideInput().
  void Convert(AudioBus* dest);
  void ConvertWithInfo(uint32_t frames_delayed,
                       const AudioGlitchInfo& glitch_info,
                       AudioBus* dest);

  // Adds or removes an input from the converter.  RemoveInput() will call
  // Reset() if no inputs remain after the specified input is removed.
  void AddInput(InputCallback* input);
  void RemoveInput(InputCallback* input);

  // Flushes all buffered data.
  void Reset();

  // The maximum size in frames that guarantees we will only make a single call
  // to each input's ProvideInput for more data.
  int ChunkSize() const;

  // See SincResampler::PrimeWithSilence.
  void PrimeWithSilence();

  // Maximum number of frames requested via InputCallback::ProvideInput, when
  // trying to convert |output_frames_requested| at a time.
  // Returns |output_frames_requested| when we are not resampling, and a
  // multiple of the request size when we are.
  int GetMaxInputFramesRequested(int output_frames_requested);

  bool empty() const { return transform_inputs_.empty(); }

 private:
  // Provides input to the MultiChannelResampler.  Called by the resampler when
  // more data is necessary.
  void ProvideInput(int resampler_frame_delay, AudioBus* audio_bus);

  // Provides input to the AudioPullFifo.  Called by the fifo when more data is
  // necessary.
  void SourceCallback(int fifo_frame_delay, AudioBus* audio_bus);

  // (Re)creates the temporary |unmixed_audio_| buffer if necessary.
  void CreateUnmixedAudioIfNecessary(int frames);

  // Set of inputs for Convert().
  typedef std::list<raw_ptr<InputCallback, CtnExperimental>> InputCallbackSet;
  InputCallbackSet transform_inputs_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  std::unique_ptr<AudioPullFifo> audio_fifo_;
  int chunk_size_;

  // Handles resampling.
  std::unique_ptr<MultiChannelResampler> resampler_;

  // Handles channel transforms.  |unmixed_audio_| is a temporary destination
  // for audio data before it goes into the channel mixer.
  std::unique_ptr<ChannelMixer> channel_mixer_;
  std::unique_ptr<AudioBus> unmixed_audio_;

  // Temporary AudioBus destination for mixing inputs.
  std::unique_ptr<AudioBus> mixer_input_audio_bus_;

  // Since resampling is expensive, figure out if we should downmix channels
  // before resampling.
  bool downmix_early_;

  // Used to calculate buffer delay information for InputCallbacks.
  uint32_t initial_frames_delayed_;
  uint32_t resampler_frames_delayed_;
  const double io_sample_rate_ratio_;

  // Number of channels of input audio data.  Set during construction via the
  // value from the input AudioParameters class.  Preserved to recreate internal
  // AudioBus structures on demand in response to varying frame size requests.
  const int input_channel_count_;

  // Accumulates glitch info in ConvertWithInfo() and passes it on to all inputs
  // in SourceCallback().
  AudioGlitchInfo::Accumulator glitch_info_accumulator_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_CONVERTER_H_
