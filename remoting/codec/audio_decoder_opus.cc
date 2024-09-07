// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/audio_decoder_opus.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "remoting/proto/audio.pb.h"
#include "third_party/opus/src/include/opus.h"

namespace remoting {

namespace {

// Maximum size of an Opus frame in milliseconds.
const int kMaxFrameSizeMs = 120;

// Hosts will never generate more than 100 frames in a single packet.
const int kMaxFramesPerPacket = 100;

const AudioPacket::SamplingRate kSamplingRate =
    AudioPacket::SAMPLING_RATE_48000;

}  // namespace

AudioDecoderOpus::AudioDecoderOpus()
    : sampling_rate_(0), channels_(0), decoder_(nullptr) {}

AudioDecoderOpus::~AudioDecoderOpus() {
  DestroyDecoder();
}

void AudioDecoderOpus::InitDecoder() {
  DCHECK(!decoder_);
  int error;
  decoder_ = opus_decoder_create(kSamplingRate, channels_, &error);
  if (!decoder_) {
    LOG(ERROR) << "Failed to create OPUS decoder; Error code: " << error;
  }
}

void AudioDecoderOpus::DestroyDecoder() {
  if (decoder_) {
    opus_decoder_destroy(decoder_);
    decoder_ = nullptr;
  }
}

bool AudioDecoderOpus::ResetForPacket(AudioPacket* packet) {
  if (packet->channels() != channels_ ||
      packet->sampling_rate() != sampling_rate_) {
    DestroyDecoder();

    channels_ = packet->channels();
    sampling_rate_ = packet->sampling_rate();

    if (channels_ <= 0 || channels_ > 2 || sampling_rate_ != kSamplingRate) {
      LOG(WARNING) << "Unsupported OPUS parameters: " << channels_
                   << " channels with " << sampling_rate_
                   << " samples per second.";
      return false;
    }
  }

  if (!decoder_) {
    InitDecoder();
  }

  return decoder_ != nullptr;
}

std::unique_ptr<AudioPacket> AudioDecoderOpus::Decode(
    std::unique_ptr<AudioPacket> packet) {
  if (packet->encoding() != AudioPacket::ENCODING_OPUS) {
    LOG(WARNING) << "Received an audio packet with encoding "
                 << packet->encoding() << " when an OPUS packet was expected.";
    return nullptr;
  }
  if (packet->data_size() > kMaxFramesPerPacket) {
    LOG(WARNING) << "Received an packet with too many frames.";
    return nullptr;
  }

  if (!ResetForPacket(packet.get())) {
    return nullptr;
  }

  // Create a new packet of decoded data.
  std::unique_ptr<AudioPacket> decoded_packet(new AudioPacket());
  decoded_packet->set_encoding(AudioPacket::ENCODING_RAW);
  decoded_packet->set_sampling_rate(kSamplingRate);
  decoded_packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  decoded_packet->set_channels(packet->channels());

  int max_frame_samples =
      kMaxFrameSizeMs * kSamplingRate / base::Time::kMillisecondsPerSecond;
  int max_frame_bytes =
      max_frame_samples * channels_ * decoded_packet->bytes_per_sample();

  std::string* decoded_data = decoded_packet->add_data();
  decoded_data->resize(packet->data_size() * max_frame_bytes);
  int buffer_pos = 0;

  for (int i = 0; i < packet->data_size(); ++i) {
    int16_t* pcm_buffer =
        reinterpret_cast<int16_t*>(std::data(*decoded_data) + buffer_pos);
    CHECK_LE(buffer_pos + max_frame_bytes,
             static_cast<int>(decoded_data->size()));
    std::string* frame = packet->mutable_data(i);
    unsigned char* frame_data =
        reinterpret_cast<unsigned char*>(std::data(*frame));
    int result = opus_decode(decoder_, frame_data, frame->size(), pcm_buffer,
                             max_frame_samples, 0);
    if (result < 0) {
      LOG(ERROR) << "Failed decoding Opus frame. Error code: " << result;
      DestroyDecoder();
      return nullptr;
    }

    buffer_pos +=
        result * packet->channels() * decoded_packet->bytes_per_sample();
  }

  if (!buffer_pos) {
    return nullptr;
  }

  decoded_data->resize(buffer_pos);

  return decoded_packet;
}

}  // namespace remoting
