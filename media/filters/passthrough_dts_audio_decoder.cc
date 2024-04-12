// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/passthrough_dts_audio_decoder.h"

#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/formats/dts/dts_util.h"

namespace media {

PassthroughDTSAudioDecoder::PassthroughDTSAudioDecoder(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    MediaLog* media_log)
    : task_runner_(task_runner),
      media_log_(media_log),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PassthroughDTSAudioDecoder::~PassthroughDTSAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

AudioDecoderType PassthroughDTSAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kPassthroughDTS;
}

void PassthroughDTSAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                            CdmContext* /* cdm_context */,
                                            InitCB init_cb,
                                            const OutputCB& output_cb,
                                            const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  InitCB bound_init_cb = base::BindPostTaskToCurrentDefault(std::move(init_cb));
  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedEncryptionMode,
                           "PassthroughDTSAudioDecoder does not support "
                           "encrypted content"));
    return;
  }

  if (config.target_output_sample_format() != kSampleFormatDts) {
    std::move(bound_init_cb)
        .Run(
            DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig,
                          "PassthroughDTSAudioDecoder does not support codec"));
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = base::BindPostTaskToCurrentDefault(output_cb);
  std::move(bound_init_cb).Run(OkStatus());
}

void PassthroughDTSAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                        DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(decode_cb);
  DecodeCB decode_cb_bound =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (buffer->end_of_stream()) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
    return;
  }

  ProcessBuffer(*buffer, std::move(decode_cb_bound));
}

void PassthroughDTSAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

void PassthroughDTSAudioDecoder::ProcessBuffer(const DecoderBuffer& buffer,
                                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure we are notified if http://crbug.com/49709 returns.  Issue also
  // occurs with some damaged files.
  if (!buffer.end_of_stream() && buffer.timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without timestamps!";
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }
  EncapsulateFrame(buffer);

  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
}

void PassthroughDTSAudioDecoder::EncapsulateFrame(const DecoderBuffer& buffer) {
  if (config_.target_output_sample_format() != kSampleFormatDts)
    return;
  const size_t samples_per_frame = dts::GetDTSSamplesPerFrame(config_.codec());
  const size_t dts_frame_size = 2 * 2 * samples_per_frame;
  std::vector<uint8_t> output_buffer(dts_frame_size);

  // Encapsulated a compressed DTS frame per IEC61937
  base::span<const uint8_t> input_data;
  input_data = base::span<const uint8_t>(buffer.data(), buffer.size());
  dts::WrapDTSWithIEC61937(input_data, output_buffer, config_.codec());

  // Create a mono channel "buffer" to hold IEC encapsulated bitstream
  uint8_t* output_channels[1] = {output_buffer.data()};
  scoped_refptr<AudioBuffer> output = AudioBuffer::CopyBitstreamFrom(
      kSampleFormatIECDts, CHANNEL_LAYOUT_MONO, 1, config_.samples_per_second(),
      samples_per_frame, output_channels, dts_frame_size, buffer.timestamp());
  output_cb_.Run(output);
}

}  // namespace media
