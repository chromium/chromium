// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/audio_encoder.h"

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/speech/audio_buffer.h"

namespace content {

namespace {

const char kContentTypeFLAC[] = "audio/x-flac; rate=";
const int kFLACCompressionLevel = 0;  // 0 for speed

FLAC__StreamEncoderWriteStatus WriteCallback(
    const FLAC__StreamEncoder* encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void* client_data) {
  AudioBuffer* encoded_audio_buffer = static_cast<AudioBuffer*>(client_data);
  encoded_audio_buffer->Enqueue(buffer, bytes);
  return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

}  // namespace

AudioEncoder::AudioEncoder(int sampling_rate, int bits_per_sample)
    : encoded_audio_buffer_(1), /* Byte granularity of encoded samples. */
      encoder_(FLAC__stream_encoder_new()),
      is_encoder_initialized_(false) {
  FLAC__stream_encoder_set_channels(encoder_, 1);
  FLAC__stream_encoder_set_bits_per_sample(encoder_, bits_per_sample);
  FLAC__stream_encoder_set_sample_rate(encoder_, sampling_rate);
  FLAC__stream_encoder_set_compression_level(encoder_, kFLACCompressionLevel);

  // Initializing the encoder will cause sync bytes to be written to
  // its output stream, so we wait until the first call to Encode()
  // before doing so.
}

AudioEncoder::~AudioEncoder() {
  FLAC__stream_encoder_delete(encoder_);
}

void AudioEncoder::Encode(const AudioChunk& raw_audio) {
  DCHECK_EQ(raw_audio.bytes_per_sample(), 2);
  if (!is_encoder_initialized_) {
    const FLAC__StreamEncoderInitStatus encoder_status =
        FLAC__stream_encoder_init_stream(encoder_, WriteCallback, nullptr,
                                         nullptr, nullptr,
                                         &encoded_audio_buffer_);
    DCHECK_EQ(encoder_status, FLAC__STREAM_ENCODER_INIT_STATUS_OK);
    is_encoder_initialized_ = true;
  }

  // FLAC encoder wants samples as int32s.
  const int num_samples = raw_audio.NumSamples();
  std::unique_ptr<FLAC__int32[]> flac_samples(new FLAC__int32[num_samples]);
  FLAC__int32* flac_samples_ptr = flac_samples.get();
  for (int i = 0; i < num_samples; ++i)
    flac_samples_ptr[i] = static_cast<FLAC__int32>(raw_audio.GetSample16(i));

  FLAC__stream_encoder_process(encoder_, &flac_samples_ptr, num_samples);
}

void AudioEncoder::Flush() {
  FLAC__stream_encoder_finish(encoder_);
}

scoped_refptr<AudioChunk> AudioEncoder::GetEncodedDataAndClear() {
  return encoded_audio_buffer_.DequeueAll();
}

std::string AudioEncoder::GetMimeType() {
  return std::string(kContentTypeFLAC) +
         base::NumberToString(FLAC__stream_encoder_get_sample_rate(encoder_));
}

int AudioEncoder::GetBitsPerSample() {
  return FLAC__stream_encoder_get_bits_per_sample(encoder_);
}

}  // namespace content
