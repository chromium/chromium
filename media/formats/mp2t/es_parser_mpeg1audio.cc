// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_parser_mpeg1audio.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bit_reader.h"
#include "media/base/channel_layout.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/common/offset_byte_queue.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

namespace media {
namespace mp2t {

struct EsParserMpeg1Audio::Mpeg1AudioFrame {
  // Pointer to the ES data.
  const uint8_t* data;

  // Frame size.
  int size;

  // Number of samples in the frame.
  int sample_count;

  // Frame offset in the ES queue.
  int64_t queue_offset;
};

EsParserMpeg1Audio::EsParserMpeg1Audio(
    const NewAudioConfigCB& new_audio_config_cb,
    const EmitBufferCB& emit_buffer_cb,
    MediaLog* media_log)
    : media_log_(media_log),
      new_audio_config_cb_(new_audio_config_cb),
      emit_buffer_cb_(emit_buffer_cb) {}

EsParserMpeg1Audio::~EsParserMpeg1Audio() {
}

bool EsParserMpeg1Audio::ParseFromEsQueue() {
  // Look for every MPEG1 audio frame in the ES buffer.
  Mpeg1AudioFrame mpeg1audio_frame;
  while (LookForMpeg1AudioFrame(&mpeg1audio_frame)) {
    // Update the audio configuration if needed.
    DCHECK_GE(mpeg1audio_frame.size, MPEG1AudioStreamParser::kHeaderSize);
    if (!UpdateAudioConfiguration(mpeg1audio_frame.data))
      return false;

    // Get the PTS & the duration of this access unit.
    TimingDesc current_timing_desc =
        GetTimingDescriptor(mpeg1audio_frame.queue_offset);
    if (current_timing_desc.pts != kNoTimestamp)
      audio_timestamp_helper_->SetBaseTimestamp(current_timing_desc.pts);

    if (audio_timestamp_helper_->base_timestamp() == kNoTimestamp) {
      DVLOG(1) << "Skipping audio frame with unknown timestamp";
      SkipMpeg1AudioFrame(mpeg1audio_frame);
      continue;
    }
    base::TimeDelta current_pts = audio_timestamp_helper_->GetTimestamp();
    base::TimeDelta frame_duration =
        audio_timestamp_helper_->GetFrameDuration(
            mpeg1audio_frame.sample_count);

    // Emit an audio frame.
    bool is_key_frame = true;

    // TODO(wolenetz/acolwell): Validate and use a common cross-parser TrackId
    // type and allow multiple audio tracks. See https://crbug.com/341581.
    scoped_refptr<StreamParserBuffer> stream_parser_buffer =
        StreamParserBuffer::CopyFrom(mpeg1audio_frame.data,
                                     mpeg1audio_frame.size, is_key_frame,
                                     DemuxerStream::AUDIO, kMp2tAudioTrackId);
    stream_parser_buffer->set_timestamp(current_pts);
    stream_parser_buffer->set_duration(frame_duration);
    emit_buffer_cb_.Run(stream_parser_buffer);

    // Update the PTS of the next frame.
    audio_timestamp_helper_->AddFrames(mpeg1audio_frame.sample_count);

    // Skip the current frame.
    SkipMpeg1AudioFrame(mpeg1audio_frame);
  }

  return true;
}

void EsParserMpeg1Audio::Flush() {
}

void EsParserMpeg1Audio::ResetInternal() {
  last_audio_decoder_config_ = AudioDecoderConfig();
}

bool EsParserMpeg1Audio::LookForMpeg1AudioFrame(
    Mpeg1AudioFrame* mpeg1audio_frame) {
  int es_size;
  const uint8_t* es;
  es_queue_->Peek(&es, &es_size);

  int max_offset = es_size - MPEG1AudioStreamParser::kHeaderSize;
  if (max_offset <= 0)
    return false;

  for (int offset = 0; offset < max_offset; offset++) {
    const uint8_t* cur_buf = &es[offset];
    if (cur_buf[0] != 0xff)
      continue;

    int remaining_size = es_size - offset;
    DCHECK_GE(remaining_size, MPEG1AudioStreamParser::kHeaderSize);
    MPEG1AudioStreamParser::Header header;
    if (!MPEG1AudioStreamParser::ParseHeader(media_log_, cur_buf, &header))
      continue;

    if (remaining_size < header.frame_size) {
      // Not a full frame: will resume when we have more data.
      // Remove all the bytes located before the frame header,
      // these bytes will not be used anymore.
      es_queue_->Pop(offset);
      return false;
    }

    // Check whether there is another frame
    // |frame_size| apart from the current one.
    if (remaining_size >= header.frame_size + 1 &&
        cur_buf[header.frame_size] != 0xff) {
      continue;
    }

    es_queue_->Pop(offset);
    es_queue_->Peek(&mpeg1audio_frame->data, &es_size);
    mpeg1audio_frame->queue_offset = es_queue_->head();
    mpeg1audio_frame->size = header.frame_size;
    mpeg1audio_frame->sample_count = header.sample_count;
    DVLOG(LOG_LEVEL_ES)
        << "MPEG1 audio syncword @ pos=" << mpeg1audio_frame->queue_offset
        << " frame_size=" << mpeg1audio_frame->size;
    DVLOG(LOG_LEVEL_ES)
        << "MPEG1 audio header: "
        << base::HexEncode(mpeg1audio_frame->data,
                           MPEG1AudioStreamParser::kHeaderSize);
    return true;
  }

  es_queue_->Pop(max_offset);
  return false;
}

bool EsParserMpeg1Audio::UpdateAudioConfiguration(
    const uint8_t* mpeg1audio_header) {
  MPEG1AudioStreamParser::Header header;
  if (!MPEG1AudioStreamParser::ParseHeader(media_log_, mpeg1audio_header,
                                           &header)) {
    return false;
  }

  // TODO(damienv): Verify whether Android playback requires the extra data
  // field for Mpeg1 audio. If yes, we should generate this field.
  AudioDecoderConfig audio_decoder_config(
      kCodecMP3, kSampleFormatS16, header.channel_layout, header.sample_rate,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);

  if (!audio_decoder_config.IsValidConfig()) {
    DVLOG(1) << "Invalid config: "
             << audio_decoder_config.AsHumanReadableString();
    return false;
  }

  if (!audio_decoder_config.Matches(last_audio_decoder_config_)) {
    DVLOG(1) << "Sampling frequency: " << header.sample_rate;
    DVLOG(1) << "Channel layout: " << header.channel_layout;
    // Reset the timestamp helper to use a new time scale.
    if (audio_timestamp_helper_ &&
        audio_timestamp_helper_->base_timestamp() != kNoTimestamp) {
      base::TimeDelta base_timestamp = audio_timestamp_helper_->GetTimestamp();
      audio_timestamp_helper_.reset(
        new AudioTimestampHelper(header.sample_rate));
      audio_timestamp_helper_->SetBaseTimestamp(base_timestamp);
    } else {
      audio_timestamp_helper_.reset(
          new AudioTimestampHelper(header.sample_rate));
    }
    // Audio config notification.
    last_audio_decoder_config_ = audio_decoder_config;
    new_audio_config_cb_.Run(audio_decoder_config);
  }

  return true;
}

void EsParserMpeg1Audio::SkipMpeg1AudioFrame(
    const Mpeg1AudioFrame& mpeg1audio_frame) {
  DCHECK_EQ(mpeg1audio_frame.queue_offset, es_queue_->head());
  es_queue_->Pop(mpeg1audio_frame.size);
}

}  // namespace mp2t
}  // namespace media
