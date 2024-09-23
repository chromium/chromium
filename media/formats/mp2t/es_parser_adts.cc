// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/es_parser_adts.h"

#include <stddef.h>

#include <optional>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bit_reader.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_pattern.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/common/offset_byte_queue.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mpeg/adts_constants.h"

namespace media {

static int ExtractAdtsFrameSize(const uint8_t* adts_header) {
  return ((static_cast<int>(adts_header[5]) >> 5) |
          (static_cast<int>(adts_header[4]) << 3) |
          ((static_cast<int>(adts_header[3]) & 0x3) << 11));
}

static int AdtsHeaderSize(const uint8_t* adts_header) {
  // protection absent bit: set to 1 if there is no CRC and 0 if there is CRC
  return (adts_header[1] & 0x1) ? kADTSHeaderSizeNoCrc : kADTSHeaderSizeWithCrc;
}

// Return true if buf corresponds to an ADTS syncword.
// |buf| size must be at least 2.
static bool isAdtsSyncWord(const uint8_t* buf) {
  // The first 12 bits must be 1.
  // The layer field (2 bits) must be set to 0.
  return (buf[0] == 0xff) && ((buf[1] & 0xf6) == 0xf0);
}

namespace mp2t {

struct EsParserAdts::AdtsFrame {
  // Pointer to the ES data.
  const uint8_t* data;

  // Frame size;
  int size;
  int header_size;

  // Frame offset in the ES queue.
  int64_t queue_offset;
};

bool EsParserAdts::LookForAdtsFrame(AdtsFrame* adts_frame) {
  int es_size;
  const uint8_t* es;
  es_queue_->Peek(&es, &es_size);

  int max_offset = es_size - kADTSHeaderMinSize;
  if (max_offset <= 0)
    return false;

  for (int offset = 0; offset < max_offset; offset++) {
    const uint8_t* cur_buf = &es[offset];
    if (!isAdtsSyncWord(cur_buf))
      continue;

    int frame_size = ExtractAdtsFrameSize(cur_buf);
    if (frame_size < kADTSHeaderMinSize) {
      // Too short to be an ADTS frame.
      continue;
    }
    int header_size = AdtsHeaderSize(cur_buf);

    int remaining_size = es_size - offset;
    if (remaining_size < frame_size) {
      // Not a full frame: will resume when we have more data.
      es_queue_->Pop(offset);
      return false;
    }

    // Check whether there is another frame
    // |size| apart from the current one.
    if (remaining_size >= frame_size + 2 &&
        !isAdtsSyncWord(&cur_buf[frame_size])) {
      continue;
    }

    es_queue_->Pop(offset);
    es_queue_->Peek(&adts_frame->data, &es_size);
    adts_frame->queue_offset = es_queue_->head();
    adts_frame->size = frame_size;
    adts_frame->header_size = header_size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS syncword @ pos=" << adts_frame->queue_offset
        << " frame_size=" << adts_frame->size;
    DVLOG(LOG_LEVEL_ES)
        << "ADTS header: "
        << base::HexEncode(adts_frame->data, kADTSHeaderMinSize);
    return true;
  }

  es_queue_->Pop(max_offset);
  return false;
}

void EsParserAdts::SkipAdtsFrame(const AdtsFrame& adts_frame) {
  DCHECK_EQ(adts_frame.queue_offset, es_queue_->head());
  es_queue_->Pop(adts_frame.size);
}

EsParserAdts::EsParserAdts(NewAudioConfigCB new_audio_config_cb,
                           EmitBufferCB emit_buffer_cb,
                           bool sbr_in_mimetype)
    : new_audio_config_cb_(std::move(new_audio_config_cb)),
      emit_buffer_cb_(std::move(emit_buffer_cb)),
      get_decrypt_config_cb_(),
      init_encryption_scheme_(EncryptionScheme::kUnencrypted),
      sbr_in_mimetype_(sbr_in_mimetype) {}

EsParserAdts::EsParserAdts(NewAudioConfigCB new_audio_config_cb,
                           EmitBufferCB emit_buffer_cb,
                           GetDecryptConfigCB get_decrypt_config_cb,
                           EncryptionScheme init_encryption_scheme,
                           bool sbr_in_mimetype)
    : new_audio_config_cb_(std::move(new_audio_config_cb)),
      emit_buffer_cb_(std::move(emit_buffer_cb)),
      get_decrypt_config_cb_(std::move(get_decrypt_config_cb)),
      init_encryption_scheme_(init_encryption_scheme),
      sbr_in_mimetype_(sbr_in_mimetype) {}

EsParserAdts::~EsParserAdts() = default;

void EsParserAdts::CalculateSubsamplesForAdtsFrame(
    const AdtsFrame& adts_frame,
    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(subsamples);
  subsamples->clear();
  int data_size = adts_frame.size - adts_frame.header_size;
  int residue = data_size % 16;
  int clear_bytes = adts_frame.header_size;
  int encrypted_bytes = 0;
  if (data_size <= 16) {
    clear_bytes += data_size;
    residue = 0;
  } else {
    clear_bytes += 16;
    encrypted_bytes = adts_frame.size - clear_bytes - residue;
  }
  SubsampleEntry subsample(clear_bytes, encrypted_bytes);
  subsamples->push_back(subsample);
  if (residue) {
    subsample.clear_bytes = residue;
    subsample.cypher_bytes = 0;
    subsamples->push_back(subsample);
  }
}

bool EsParserAdts::ParseFromEsQueue() {
  // Look for every ADTS frame in the ES buffer.
  AdtsFrame adts_frame;
  while (LookForAdtsFrame(&adts_frame)) {
    // Update the audio configuration if needed.
    DCHECK_GE(adts_frame.size, kADTSHeaderMinSize);
    if (!UpdateAudioConfiguration(adts_frame.data, adts_frame.size))
      return false;

    // Get the PTS & the duration of this access unit.
    TimingDesc current_timing_desc =
        GetTimingDescriptor(adts_frame.queue_offset);
    if (current_timing_desc.pts != kNoTimestamp)
      audio_timestamp_helper_->SetBaseTimestamp(current_timing_desc.pts);

    if (!audio_timestamp_helper_->base_timestamp()) {
      DVLOG(1) << "Skipping audio frame with unknown timestamp";
      SkipAdtsFrame(adts_frame);
      continue;
    }
    base::TimeDelta current_pts = audio_timestamp_helper_->GetTimestamp();
    base::TimeDelta frame_duration =
        audio_timestamp_helper_->GetFrameDuration(kSamplesPerAACFrame);

    // Emit an audio frame.
    bool is_key_frame = true;

    // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
    // type and allow multiple audio tracks. See https://crbug.com/341581.
    scoped_refptr<StreamParserBuffer> stream_parser_buffer =
        StreamParserBuffer::CopyFrom(adts_frame.data, adts_frame.size,
                                     is_key_frame, DemuxerStream::AUDIO,
                                     kMp2tAudioTrackId);
    stream_parser_buffer->set_timestamp(current_pts);
    stream_parser_buffer->SetDecodeTimestamp(
        DecodeTimestamp::FromPresentationTime(current_pts));
    stream_parser_buffer->set_duration(frame_duration);
    if (get_decrypt_config_cb_) {
      const DecryptConfig* base_decrypt_config = get_decrypt_config_cb_.Run();
      if (base_decrypt_config) {
        std::vector<SubsampleEntry> subsamples;
        CalculateSubsamplesForAdtsFrame(adts_frame, &subsamples);
        stream_parser_buffer->set_decrypt_config(
            std::make_unique<DecryptConfig>(
                base_decrypt_config->encryption_scheme(),
                base_decrypt_config->key_id(), base_decrypt_config->iv(),
                subsamples, EncryptionPattern()));
      }
    }
    emit_buffer_cb_.Run(stream_parser_buffer);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(kSamplesPerAACFrame);

    // Skip the current frame.
    SkipAdtsFrame(adts_frame);
  }

  return true;
}

void EsParserAdts::Flush() {
}

void EsParserAdts::ResetInternal() {
  last_audio_decoder_config_ = AudioDecoderConfig();
}

bool EsParserAdts::UpdateAudioConfiguration(const uint8_t* adts_header,
                                            int size) {
  int orig_sample_rate;
  ChannelLayout channel_layout;
  std::vector<uint8_t> extra_data;
  if (adts_parser_.ParseFrameHeader(adts_header, size, nullptr,
                                    &orig_sample_rate, &channel_layout, nullptr,
                                    nullptr, &extra_data) <= 0) {
    return false;
  }

  // The following code is written according to ISO 14496 Part 3 Table 1.11 and
  // Table 1.22. (Table 1.11 refers to the capping to 48000, Table 1.22 refers
  // to SBR doubling the AAC sample rate.)
  // TODO(damienv) : Extend sample rate cap to 96kHz for Level 5 content.
  const int extended_samples_per_second =
      sbr_in_mimetype_ ? std::min(2 * orig_sample_rate, 48000)
                       : orig_sample_rate;
  AudioDecoderConfig audio_decoder_config(
      AudioCodec::kAAC, kSampleFormatS16, channel_layout,
      extended_samples_per_second, extra_data, init_encryption_scheme_);

  if (!audio_decoder_config.IsValidConfig()) {
    DVLOG(1) << "Invalid config: "
             << audio_decoder_config.AsHumanReadableString();
    return false;
  }

  if (!audio_decoder_config.Matches(last_audio_decoder_config_)) {
    DVLOG(1) << "Sampling frequency: "
             << audio_decoder_config.samples_per_second()
             << " SBR=" << sbr_in_mimetype_;
    DVLOG(1) << "Channel layout: "
             << ChannelLayoutToString(audio_decoder_config.channel_layout());

    // For AAC audio with SBR (Spectral Band Replication) the sampling rate is
    // doubled above, but AudioTimestampHelper should still use the original
    // sample rate to compute audio timestamps and durations correctly.

    // Reset the timestamp helper to use a new time scale.
    if (audio_timestamp_helper_ && audio_timestamp_helper_->base_timestamp()) {
      base::TimeDelta base_timestamp = audio_timestamp_helper_->GetTimestamp();
      audio_timestamp_helper_.reset(new AudioTimestampHelper(orig_sample_rate));
      audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
    } else {
      audio_timestamp_helper_.reset(new AudioTimestampHelper(orig_sample_rate));
    }
    // Audio config notification.
    last_audio_decoder_config_ = audio_decoder_config;
    new_audio_config_cb_.Run(audio_decoder_config);
  }

  return true;
}

}  // namespace mp2t
}  // namespace media
