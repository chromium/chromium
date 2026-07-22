// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/tracks_builder.h"

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

// Returns size of an integer, formatted using Matroska serialization.
static int GetUIntMkvSize(uint64_t value) {
  if (value < 0x07FULL)
    return 1;
  if (value < 0x03FFFULL)
    return 2;
  if (value < 0x01FFFFFULL)
    return 3;
  if (value < 0x0FFFFFFFULL)
    return 4;
  if (value < 0x07FFFFFFFFULL)
    return 5;
  if (value < 0x03FFFFFFFFFFULL)
    return 6;
  if (value < 0x01FFFFFFFFFFFFULL)
    return 7;
  return 8;
}

// Returns the minimum size required to serialize an integer value.
static int GetUIntSize(uint64_t value) {
  if (value < 0x0100ULL)
    return 1;
  if (value < 0x010000ULL)
    return 2;
  if (value < 0x01000000ULL)
    return 3;
  if (value < 0x0100000000ULL)
    return 4;
  if (value < 0x010000000000ULL)
    return 5;
  if (value < 0x01000000000000ULL)
    return 6;
  if (value < 0x0100000000000000ULL)
    return 7;
  return 8;
}

static int MasterElementSize(int element_id, int payload_size) {
  return GetUIntSize(element_id) + GetUIntMkvSize(payload_size) + payload_size;
}

static int UIntElementSize(int element_id, uint64_t value) {
  return GetUIntSize(element_id) + 1 + GetUIntSize(value);
}

static int DoubleElementSize(int element_id) {
  return GetUIntSize(element_id) + 1 + 8;
}

static int StringElementSize(int element_id, const std::string& value) {
 return GetUIntSize(element_id) +
        GetUIntMkvSize(value.length()) +
        value.length();
}

static void SerializeInt(base::SpanWriter<uint8_t>& writer,
                         int64_t value,
                         int size) {
  for (int idx = 1; idx <= size; ++idx) {
    CHECK(writer.Write(static_cast<uint8_t>(value >> ((size - idx) * 8))));
  }
}

static void SerializeDouble(base::SpanWriter<uint8_t>& writer, double value) {
  // Reinterpret `value`'s bit pattern as an integer, then write those bytes in
  // big-endian form.
  SerializeInt(writer, base::bit_cast<int64_t>(value), 8);
}

static void WriteElementId(base::SpanWriter<uint8_t>& writer, int element_id) {
  SerializeInt(writer, element_id, GetUIntSize(element_id));
}

static void WriteUInt(base::SpanWriter<uint8_t>& writer, uint64_t value) {
  const int size = GetUIntMkvSize(value);
  value |= (1ULL << (size * 7));  // Matroska formatting
  SerializeInt(writer, value, size);
}

static void WriteMasterElement(base::SpanWriter<uint8_t>& writer,
                               int element_id,
                               int payload_size) {
  WriteElementId(writer, element_id);
  WriteUInt(writer, payload_size);
}

static void WriteUIntElement(base::SpanWriter<uint8_t>& writer,
                             int element_id,
                             uint64_t value) {
  WriteElementId(writer, element_id);

  const int size = GetUIntSize(value);
  WriteUInt(writer, size);

  SerializeInt(writer, value, size);
}

static void WriteDoubleElement(base::SpanWriter<uint8_t>& writer,
                               int element_id,
                               double value) {
  WriteElementId(writer, element_id);
  WriteUInt(writer, 8);
  SerializeDouble(writer, value);
}

static void WriteStringElement(base::SpanWriter<uint8_t>& writer,
                               int element_id,
                               const std::string& value) {
  WriteElementId(writer, element_id);

  const uint64_t size = value.length();
  WriteUInt(writer, size);

  CHECK(writer.Write(base::as_byte_span(value)));
}

TracksBuilder::TracksBuilder(bool allow_invalid_values)
    : allow_invalid_values_(allow_invalid_values) {}
TracksBuilder::TracksBuilder()
    : allow_invalid_values_(false) {}
TracksBuilder::~TracksBuilder() = default;

void TracksBuilder::AddVideoTrack(int track_num,
                                  uint64_t track_uid,
                                  const std::string& codec_id,
                                  const std::string& name,
                                  const std::string& language,
                                  int default_duration,
                                  int video_pixel_width,
                                  int video_pixel_height) {
  AddTrackInternal(track_num, kWebMTrackTypeVideo, track_uid, codec_id, name,
                   language, default_duration, video_pixel_width,
                   video_pixel_height, -1, -1);
}

void TracksBuilder::AddAudioTrack(int track_num,
                                  uint64_t track_uid,
                                  const std::string& codec_id,
                                  const std::string& name,
                                  const std::string& language,
                                  int default_duration,
                                  int audio_channels,
                                  double audio_sampling_frequency) {
  AddTrackInternal(track_num, kWebMTrackTypeAudio, track_uid, codec_id, name,
                   language, default_duration, -1, -1, audio_channels,
                   audio_sampling_frequency);
}

void TracksBuilder::AddTextTrack(int track_num,
                                 uint64_t track_uid,
                                 const std::string& codec_id,
                                 const std::string& name,
                                 const std::string& language) {
  AddTrackInternal(track_num, kWebMTrackTypeSubtitlesOrCaptions, track_uid,
                   codec_id, name, language, -1, -1, -1, -1, -1);
}

std::vector<uint8_t> TracksBuilder::Finish() {
  // Allocate the storage.
  std::vector<uint8_t> buffer;
  buffer.resize(GetTracksSize());

  // Populate the storage with a tracks header.
  auto writer = base::SpanWriter(base::span(buffer));
  WriteTracks(writer);
  CHECK_EQ(writer.remaining(), 0u);

  return buffer;
}

void TracksBuilder::AddTrackInternal(int track_num,
                                     int track_type,
                                     uint64_t track_uid,
                                     const std::string& codec_id,
                                     const std::string& name,
                                     const std::string& language,
                                     int default_duration,
                                     int video_pixel_width,
                                     int video_pixel_height,
                                     int audio_channels,
                                     double audio_sampling_frequency) {
  tracks_.push_back(Track(track_num, track_type, track_uid, codec_id, name,
                          language, default_duration, video_pixel_width,
                          video_pixel_height, audio_channels,
                          audio_sampling_frequency, allow_invalid_values_));
}

int TracksBuilder::GetTracksSize() const {
  return MasterElementSize(kWebMIdTracks, GetTracksPayloadSize());
}

int TracksBuilder::GetTracksPayloadSize() const {
  int payload_size = 0;

  for (auto itr = tracks_.begin(); itr != tracks_.end(); ++itr) {
    payload_size += itr->GetSize();
  }

  return payload_size;
}

void TracksBuilder::WriteTracks(base::SpanWriter<uint8_t>& writer) const {
  WriteMasterElement(writer, kWebMIdTracks, GetTracksPayloadSize());

  for (const auto& track : tracks_) {
    track.Write(writer);
  }
}

TracksBuilder::Track::Track(int track_num,
                            int track_type,
                            uint64_t track_uid,
                            const std::string& codec_id,
                            const std::string& name,
                            const std::string& language,
                            int default_duration,
                            int video_pixel_width,
                            int video_pixel_height,
                            int audio_channels,
                            double audio_sampling_frequency,
                            bool allow_invalid_values)
    : track_num_(track_num),
      track_type_(track_type),
      track_uid_(track_uid),
      codec_id_(codec_id),
      name_(name),
      language_(language),
      default_duration_(default_duration),
      video_pixel_width_(video_pixel_width),
      video_pixel_height_(video_pixel_height),
      audio_channels_(audio_channels),
      audio_sampling_frequency_(audio_sampling_frequency) {
  if (!allow_invalid_values) {
    CHECK_GT(track_num_, 0);
    CHECK_GT(track_type_, 0);
    CHECK_LT(track_type_, 255);
    CHECK_GT(track_uid_, 0);
    if (track_type != kWebMTrackTypeVideo &&
        track_type != kWebMTrackTypeAudio) {
      CHECK_EQ(default_duration_, -1);
    } else {
      CHECK(default_duration_ == -1 || default_duration_ > 0);
    }

    if (track_type == kWebMTrackTypeVideo) {
      CHECK_GT(video_pixel_width_, 0);
      CHECK_GT(video_pixel_height_, 0);
    } else {
      CHECK_EQ(video_pixel_width_, -1);
      CHECK_EQ(video_pixel_height_, -1);
    }

    if (track_type == kWebMTrackTypeAudio) {
      CHECK_GT(audio_channels_, 0);
      CHECK_GT(audio_sampling_frequency_, 0.0);
    } else {
      CHECK_EQ(audio_channels_, -1);
      CHECK_EQ(audio_sampling_frequency_, -1.0);
    }
  }
}

TracksBuilder::Track::Track(const Track& other) = default;

int TracksBuilder::Track::GetSize() const {
  return MasterElementSize(kWebMIdTrackEntry, GetPayloadSize());
}

int TracksBuilder::Track::GetVideoPayloadSize() const {
  int payload_size = 0;

  if (video_pixel_width_ >= 0)
    payload_size += UIntElementSize(kWebMIdPixelWidth, video_pixel_width_);
  if (video_pixel_height_ >= 0)
    payload_size += UIntElementSize(kWebMIdPixelHeight, video_pixel_height_);

  return payload_size;
}

int TracksBuilder::Track::GetAudioPayloadSize() const {
  int payload_size = 0;

  if (audio_channels_ >= 0)
    payload_size += UIntElementSize(kWebMIdChannels, audio_channels_);
  if (audio_sampling_frequency_ >= 0)
    payload_size += DoubleElementSize(kWebMIdSamplingFrequency);

  return payload_size;
}

int TracksBuilder::Track::GetPayloadSize() const {
  int size = 0;

  size += UIntElementSize(kWebMIdTrackNumber, track_num_);
  size += UIntElementSize(kWebMIdTrackType, track_type_);
  size += UIntElementSize(kWebMIdTrackUID, track_uid_);

  if (default_duration_ >= 0)
    size += UIntElementSize(kWebMIdDefaultDuration, default_duration_);

  if (!codec_id_.empty())
    size += StringElementSize(kWebMIdCodecID, codec_id_);

  if (!name_.empty())
    size += StringElementSize(kWebMIdName, name_);

  if (!language_.empty())
    size += StringElementSize(kWebMIdLanguage, language_);

  if (GetVideoPayloadSize() > 0) {
    size += MasterElementSize(kWebMIdVideo, GetVideoPayloadSize());
  }

  if (GetAudioPayloadSize() > 0) {
    size += MasterElementSize(kWebMIdAudio, GetAudioPayloadSize());
  }

  return size;
}

void TracksBuilder::Track::Write(base::SpanWriter<uint8_t>& writer) const {
  WriteMasterElement(writer, kWebMIdTrackEntry, GetPayloadSize());

  WriteUIntElement(writer, kWebMIdTrackNumber, track_num_);
  WriteUIntElement(writer, kWebMIdTrackType, track_type_);
  WriteUIntElement(writer, kWebMIdTrackUID, track_uid_);

  if (default_duration_ >= 0)
    WriteUIntElement(writer, kWebMIdDefaultDuration, default_duration_);

  if (!codec_id_.empty())
    WriteStringElement(writer, kWebMIdCodecID, codec_id_);

  if (!name_.empty())
    WriteStringElement(writer, kWebMIdName, name_);

  if (!language_.empty())
    WriteStringElement(writer, kWebMIdLanguage, language_);

  if (GetVideoPayloadSize() > 0) {
    WriteMasterElement(writer, kWebMIdVideo, GetVideoPayloadSize());

    if (video_pixel_width_ >= 0)
      WriteUIntElement(writer, kWebMIdPixelWidth, video_pixel_width_);

    if (video_pixel_height_ >= 0)
      WriteUIntElement(writer, kWebMIdPixelHeight, video_pixel_height_);
  }

  if (GetAudioPayloadSize() > 0) {
    WriteMasterElement(writer, kWebMIdAudio, GetAudioPayloadSize());

    if (audio_channels_ >= 0)
      WriteUIntElement(writer, kWebMIdChannels, audio_channels_);

    if (audio_sampling_frequency_ >= 0) {
      WriteDoubleElement(writer, kWebMIdSamplingFrequency,
                         audio_sampling_frequency_);
    }
  }
}

}  // namespace media
