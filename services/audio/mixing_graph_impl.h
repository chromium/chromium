// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_MIXING_GRAPH_IMPL_H_
#define SERVICES_AUDIO_MIXING_GRAPH_IMPL_H_

#include "base/gtest_prod_util.h"
#include "services/audio/mixing_graph.h"

#include <map>

#include "base/synchronization/lock.h"

namespace media {
class LoopbackAudioConverter;
}

namespace audio {

class MixingGraphImpl : public MixingGraph {
 public:
  using CreateConverterCallback =
      base::RepeatingCallback<std::unique_ptr<media::LoopbackAudioConverter>(
          const media::AudioParameters& input_params,
          const media::AudioParameters& output_params)>;
  MixingGraphImpl(const media::AudioParameters& output_params,
                  OnMoreDataCallback on_more_data_cb,
                  OnErrorCallback on_error_cb);
  MixingGraphImpl(const media::AudioParameters& output_params,
                  OnMoreDataCallback on_more_data_cb,
                  OnErrorCallback on_error_cb,
                  CreateConverterCallback create_converter_cb);
  ~MixingGraphImpl() override;

  std::unique_ptr<Input> CreateInput(
      const media::AudioParameters& params) final;
  void AddInput(Input* node) final;
  void RemoveInput(Input* node) final;

  // media::AudioOutputStream::AudioSourceCallback
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const media::AudioGlitchInfo& glitch_info,
                 media::AudioBus* dest) final;

  void OnError(ErrorType type) final;

 protected:
  class OvertimeLogger;

  media::LoopbackAudioConverter* FindOrAddConverter(
      const media::AudioParameters& input_params,
      const media::AudioParameters& output_params,
      media::LoopbackAudioConverter* parent_converter);

  class AudioConverterKey {
   public:
    explicit AudioConverterKey(const media::AudioParameters& params)
        : sample_rate_(params.sample_rate()),
          channel_layout_(params.channel_layout()),
          channels_(params.channels()) {}

    inline bool operator==(const AudioConverterKey& other) const {
      return sample_rate_ == other.sample_rate_ &&
             channel_layout_ == other.channel_layout_ &&
             channels_ == other.channels_;
    }

    inline bool operator<(const AudioConverterKey& other) const {
      if (sample_rate_ != other.sample_rate_)
        return sample_rate_ < other.sample_rate_;
      if (channel_layout_ != other.channel_layout_)
        return channel_layout_ < other.channel_layout_;
      return channels_ < other.channels_;
    }

    inline bool SameChannelSetup(const media::AudioParameters& params) const {
      return channel_layout_ == params.channel_layout() &&
             channels_ == params.channels();
    }

    inline void UpdateChannelSetup(const media::AudioParameters& params) {
      channel_layout_ = params.channel_layout();
      channels_ = params.channels();
    }

    inline int sample_rate() const { return sample_rate_; }

    inline void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }

   private:
    int sample_rate_;
    media::ChannelLayout channel_layout_;
    int channels_;
  };
  FRIEND_TEST_ALL_PREFIXES(MixingGraphImpl, AudioConverterKeySorting);

  void Remove(const AudioConverterKey& key,
              media::AudioConverter::InputCallback* input);

  using AudioConverters =
      std::map<AudioConverterKey,
               std::unique_ptr<media::LoopbackAudioConverter>>;

  SEQUENCE_CHECKER(owning_sequence_);

  // The audio format of the mixed audio leaving the mixing graph.
  const media::AudioParameters output_params_;
  // Called from OnMoreData() with mixed audio as input.
  const OnMoreDataCallback on_more_data_cb_;
  // Notifies the client about audio rendering errors.
  const OnErrorCallback on_error_cb_;
  // Called when a new converter needs to be created.
  const CreateConverterCallback create_converter_cb_;

  // UMA logging.
  const std::unique_ptr<OvertimeLogger> overtime_logger_;

  base::Lock lock_;

  // The |main_converter_|, the |converters_| and the inputs are connected
  // to form a graph (tree structure) that determines how the input audio is
  // channel mixed, resampled, and added to the final output. Channel mixing
  // and resampling are handled by converters. The tree is constructed to
  // minimize the use of resampling, which is the most complex operation.
  // 1. For inputs with a channel layout different from the output channel
  // layout: All inputs of the same sample rate and channel layout are
  // combined and channel mixed to produce new inputs with the output
  // channel layout.
  // 2. For inputs with a sample rate different from the output sample rate:
  // All inputs of the same sample rate (and after channel mixing the same
  // channel layout) are combined and resampled to produce new inputs of the
  // output sample rate (and channel layout).
  // 3. All inputs of the output sample rate and channel layout are combined
  // by the main converter to produce a single output.
  AudioConverters converters_;
  media::AudioConverter main_converter_;
};

}  // namespace audio
#endif  // SERVICES_AUDIO_MIXING_GRAPH_IMPL_H_
