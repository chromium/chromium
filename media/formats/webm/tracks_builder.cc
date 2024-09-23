// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/tracks_builder.h"

#include <cstring>

#include "base/check_op.h"
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

static void SerializeInt(uint8_t** buf_ptr,
                         int* buf_size_ptr,
                         int64_t value,
                         int size) {
  uint8_t*& buf = *buf_ptr;
  int& buf_size = *buf_size_ptr;

  for (int idx = 1; idx <= size; ++idx) {
    *buf++ = static_cast<uint8_t>(value >> ((size - idx) * 8));
    --buf_size;
  }
}

static void SerializeDouble(uint8_t** buf_ptr,
                            int* buf_size_ptr,
                            double value) {
  // Use a union to convert |value| to native endian integer bit pattern.
  union {
    double src;
    int64_t dst;
  } tmp;
  tmp.src = value;

  // Write the bytes from native endian |tmp.dst| to big-endian form in |buf|.
  SerializeInt(buf_ptr, buf_size_ptr, tmp.dst, 8);
}

static void WriteElementId(uint8_t** buf, int* buf_size, int element_id) {
  SerializeInt(buf, buf_size, element_id, GetUIntSize(element_id));
}

static void WriteUInt(uint8_t** buf, int* buf_size, uint64_t value) {
  const int size = GetUIntMkvSize(value);
  value |= (1ULL << (size * 7));  // Matroska formatting
  SerializeInt(buf, buf_size, value, size);
}

static void WriteMasterElement(uint8_t** buf,
                               int* buf_size,
                               int element_id,
                               int payload_size) {
  WriteElementId(buf, buf_size, element_id);
  WriteUInt(buf, buf_size, payload_size);
}

static void WriteUIntElement(uint8_t** buf,
                             int* buf_size,
                             int element_id,
                             uint64_t value) {
  WriteElementId(buf, buf_size, element_id);

  const int size = GetUIntSize(value);
  WriteUInt(buf, buf_size, size);

  SerializeInt(buf, buf_size, value, size);
}

static void WriteDoubleElement(uint8_t** buf,
                               int* buf_size,
                               int element_id,
                               double value) {
  WriteElementId(buf, buf_size, element_id);
  WriteUInt(buf, buf_size, 8);
  SerializeDouble(buf, buf_size, value);
}

static void WriteStringElement(uint8_t** buf_ptr,
                               int* buf_size_ptr,
                               int element_id,
                               const std::string& value) {
  uint8_t*& buf = *buf_ptr;
  int& buf_size = *buf_size_ptr;

  WriteElementId(&buf, &buf_size, element_id);

  const uint64_t size = value.length();
  WriteUInt(&buf, &buf_size, size);

  memcpy(buf, value.data(), size);
  buf += size;
  buf_size -= size;
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
  // Allocate the storage
  std::vector<uint8_t> buffer;
  buffer.resize(GetTracksSize());

  // Populate the storage with a tracks header
  WriteTracks(&buffer[0], buffer.size());

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

void TracksBuilder::WriteTracks(uint8_t* buf, int buf_size) const {
  WriteMasterElement(&buf, &buf_size, kWebMIdTracks, GetTracksPayloadSize());

  for (auto itr = tracks_.begin(); itr != tracks_.end(); ++itr) {
    itr->Write(&buf, &buf_size);
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

void TracksBuilder::Track::Write(uint8_t** buf, int* buf_size) const {
  WriteMasterElement(buf, buf_size, kWebMIdTrackEntry, GetPayloadSize());

  WriteUIntElement(buf, buf_size, kWebMIdTrackNumber, track_num_);
  WriteUIntElement(buf, buf_size, kWebMIdTrackType, track_type_);
  WriteUIntElement(buf, buf_size, kWebMIdTrackUID, track_uid_);

  if (default_duration_ >= 0)
    WriteUIntElement(buf, buf_size, kWebMIdDefaultDuration, default_duration_);

  if (!codec_id_.empty())
    WriteStringElement(buf, buf_size, kWebMIdCodecID, codec_id_);

  if (!name_.empty())
    WriteStringElement(buf, buf_size, kWebMIdName, name_);

  if (!language_.empty())
    WriteStringElement(buf, buf_size, kWebMIdLanguage, language_);

  if (GetVideoPayloadSize() > 0) {
    WriteMasterElement(buf, buf_size, kWebMIdVideo, GetVideoPayloadSize());

    if (video_pixel_width_ >= 0)
      WriteUIntElement(buf, buf_size, kWebMIdPixelWidth, video_pixel_width_);

    if (video_pixel_height_ >= 0)
      WriteUIntElement(buf, buf_size, kWebMIdPixelHeight, video_pixel_height_);
  }

  if (GetAudioPayloadSize() > 0) {
    WriteMasterElement(buf, buf_size, kWebMIdAudio, GetAudioPayloadSize());

    if (audio_channels_ >= 0)
      WriteUIntElement(buf, buf_size, kWebMIdChannels, audio_channels_);

    if (audio_sampling_frequency_ >= 0) {
      WriteDoubleElement(buf, buf_size, kWebMIdSamplingFrequency,
          audio_sampling_frequency_);
    }
  }
}

}  // namespace media
