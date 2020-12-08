// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_audio_encoder.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_descriptor.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_packetizer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

namespace extensions {

namespace {

using LPCMAudioStreamDescriptor =
    WiFiDisplayElementaryStreamDescriptor::LPCMAudioStream;

// This audio encoder implements Linear Pulse-Code Modulation (LPCM) audio
// encoding.
class WiFiDisplayAudioEncoderLPCM final
    : public WiFiDisplayAudioEncoder,
      private media::AudioConverter::InputCallback {
 public:
  enum {
    kOutputChannels = 2
  };

  explicit WiFiDisplayAudioEncoderLPCM(const wds::AudioCodec& audio_codec);

 protected:
  ~WiFiDisplayAudioEncoderLPCM() override = default;

  // WiFiDisplayMediaEncoder
  WiFiDisplayElementaryStreamInfo CreateElementaryStreamInfo() const override;

  // blink::WebMediaStreamAudioSink
  void OnData(const media::AudioBus& input_bus,
              base::TimeTicks estimated_capture_time) override;
  void OnSetFormat(const media::AudioParameters& params) override;

  // media::AudioConverter::InputCallback
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override;

  LPCMAudioStreamDescriptor::SamplingFrequency GetOutputSamplingFrequency()
      const;

 private:
  const int output_sample_rate_;

  // These members are accessed on the real-time audio time only.
  std::unique_ptr<media::AudioConverter> converter_;
  const media::AudioBus* current_input_bus_;
  int64_t input_frames_in_;
  media::AudioParameters input_params_;
  std::unique_ptr<media::AudioBus> fifo_bus_;
  int fifo_end_frame_;
  int64_t fifo_frames_out_;
};

WiFiDisplayAudioEncoderLPCM::WiFiDisplayAudioEncoderLPCM(
    const wds::AudioCodec& audio_codec)
    : WiFiDisplayAudioEncoder(audio_codec),
      output_sample_rate_(
          GetOutputSamplingFrequency() ==
                  LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_48K
              ? 48000
              : 44100),
      current_input_bus_(nullptr),
      input_frames_in_(0),
      fifo_end_frame_(0),
      fifo_frames_out_(0) {}

WiFiDisplayElementaryStreamInfo
WiFiDisplayAudioEncoderLPCM::CreateElementaryStreamInfo() const {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  std::vector<WiFiDisplayElementaryStreamDescriptor> descriptors;
  descriptors.push_back(LPCMAudioStreamDescriptor::Create(
      GetOutputSamplingFrequency(),
      LPCMAudioStreamDescriptor::BITS_PER_SAMPLE_16,
      false,  // emphasis_flag
      LPCMAudioStreamDescriptor::NUMBER_OF_CHANNELS_STEREO));
  return WiFiDisplayElementaryStreamInfo(
      WiFiDisplayElementaryStreamInfo::AUDIO_LPCM, std::move(descriptors));
}

// Called on real-time audio thread.
void WiFiDisplayAudioEncoderLPCM::OnData(
    const media::AudioBus& input_bus,
    base::TimeTicks estimated_capture_time) {
  DCHECK(input_params_.IsValid());
  DCHECK_EQ(input_bus.channels(), input_params_.channels());
  DCHECK_EQ(input_bus.frames(), input_params_.frames_per_buffer());
  DCHECK(!estimated_capture_time.is_null());

  const media::AudioBus* source_bus = &input_bus;

  std::unique_ptr<media::AudioBus> converted_input_bus;
  if (converter_) {
    // Convert the entire input signal.
    // Note that while the number of sample frames provided as input is always
    // the same, the chunk size (and the size of the |converted_input_bus|
    // here) can be variable.
    converted_input_bus =
        media::AudioBus::Create(kOutputChannels, converter_->ChunkSize());
    current_input_bus_ = &input_bus;
    converter_->Convert(converted_input_bus.get());
    DCHECK(!current_input_bus_);
    source_bus = converted_input_bus.get();
  }

  // Loop in order to handle frame number differences between |source_bus| and
  // |fifo_bus_|.
  int source_start_frame = 0;
  do {
    // Copy as many source frames (either raw or converted input frames) as
    // possible to |fifo_bus_|.
    int frame_count = std::min(source_bus->frames() - source_start_frame,
                               fifo_bus_->frames() - fifo_end_frame_);
    DCHECK_GT(frame_count, 0);
    source_bus->CopyPartialFramesTo(source_start_frame, frame_count,
                                    fifo_end_frame_, fifo_bus_.get());
    fifo_end_frame_ += frame_count;
    source_start_frame += frame_count;

    if (fifo_end_frame_ == fifo_bus_->frames()) {
      // There are enough frames in |fifo_bus_| for one encoded unit.

      // Determine the duration of the audio signal enqueued within
      // |converter_| and |fifo_bus_|.
      const base::TimeDelta signal_duration_already_buffered =
          (input_frames_in_ * base::TimeDelta::FromSeconds(1) /
           input_params_.sample_rate()) -
          (fifo_frames_out_ * base::TimeDelta::FromSeconds(1) /
           output_sample_rate_);
      DVLOG(2) << "Audio reference time adjustment: -("
               << signal_duration_already_buffered.InMicroseconds() << " us)";
      const base::TimeTicks capture_time_of_first_converted_sample =
          estimated_capture_time - signal_duration_already_buffered;

      // Encode frames in |fifo_bus_|.
      std::string data;
      int sample_count = fifo_bus_->channels() * fifo_bus_->frames();
      data.resize(sample_count * sizeof(uint16_t));
      uint16_t* encoded_samples = reinterpret_cast<uint16_t*>(base::data(data));
      fifo_bus_->ToInterleaved<media::SignedInt16SampleTypeTraits>(
          fifo_bus_->frames(), encoded_samples);
      for (int i = 0; i < sample_count; ++i)
        encoded_samples[i] = base::HostToNet16(encoded_samples[i]);
      fifo_end_frame_ = 0;
      fifo_frames_out_ += fifo_bus_->frames();

      // Pass the encoded unit to the client.
      encoded_callback_.Run(
          std::unique_ptr<WiFiDisplayEncodedUnit>(new WiFiDisplayEncodedUnit(
              std::move(data), capture_time_of_first_converted_sample, true)));
    }
  } while (source_start_frame < source_bus->frames());
}

// Called on real-time audio thread.
void WiFiDisplayAudioEncoderLPCM::OnSetFormat(
    const media::AudioParameters& params) {
  if (input_params_.Equals(params))
    return;
  input_params_ = params;

  if (output_sample_rate_ != input_params_.sample_rate()) {
    const media::AudioParameters output_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO, output_sample_rate_,
        WiFiDisplayMediaPacketizer::LPCM::kChannelSamplesPerUnit);
    DVLOG(2) << "Setting up audio resampling: "
             << input_params_.sample_rate() << " Hz --> "
             << output_sample_rate_ << " Hz";
    converter_.reset(
        new media::AudioConverter(input_params_, output_params, false));
    converter_->AddInput(this);
  } else {
    // Do not use an AudioConverter if that is not needed in order to avoid
    // additional copyings caused by additional intermediate audio busses
    // needed for capturing enough samples for each encoded unit.
    converter_.reset();
  }

  fifo_bus_ = media::AudioBus::Create(
      kOutputChannels,
      WiFiDisplayMediaPacketizer::LPCM::kChannelSamplesPerUnit);
  fifo_end_frame_ = 0;
  fifo_frames_out_ = 0;
  input_frames_in_ = 0;
}

// Called on real-time audio thread by |converter_| invoked by |OnData|.
double WiFiDisplayAudioEncoderLPCM::ProvideInput(media::AudioBus* audio_bus,
                                                 uint32_t frames_delayed) {
  DCHECK(current_input_bus_);
  current_input_bus_->CopyTo(audio_bus);
  current_input_bus_ = nullptr;
  return 1.0;
}

LPCMAudioStreamDescriptor::SamplingFrequency
WiFiDisplayAudioEncoderLPCM::GetOutputSamplingFrequency() const {
  switch (GetAudioCodecMode()) {
    case wds::LPCM_44_1K_16B_2CH:
      return LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_44_1K;
    case wds::LPCM_48K_16B_2CH:
      return LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_48K;
    default:
      NOTREACHED();
      return LPCMAudioStreamDescriptor::SAMPLING_FREQUENCY_44_1K;
  }
}

}  // namespace

void WiFiDisplayAudioEncoder::CreateLPCM(
    const wds::AudioCodec& audio_codec,
    AudioEncoderCallback encoder_callback) {
  std::move(encoder_callback)
      .Run(base::MakeRefCounted<WiFiDisplayAudioEncoderLPCM>(audio_codec));
}

}  // namespace extensions
