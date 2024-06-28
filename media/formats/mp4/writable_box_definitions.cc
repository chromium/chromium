// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/writable_box_definitions.h"

namespace media::mp4::writable_boxes {

Movie::Movie() = default;
Movie::~Movie() = default;

MovieExtends::MovieExtends() = default;
MovieExtends::~MovieExtends() = default;

MovieHeader::MovieHeader() = default;
MovieHeader::~MovieHeader() = default;

DataReference::DataReference() = default;
DataReference::~DataReference() = default;
DataReference::DataReference(const DataReference&) = default;
DataReference& DataReference::operator=(const DataReference&) = default;

SampleDescription::SampleDescription() = default;
SampleDescription::~SampleDescription() = default;
SampleDescription::SampleDescription(const SampleDescription&) = default;
SampleDescription& SampleDescription::operator=(const SampleDescription&) =
    default;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
ElementaryStreamDescriptor::ElementaryStreamDescriptor() = default;
ElementaryStreamDescriptor::~ElementaryStreamDescriptor() = default;
ElementaryStreamDescriptor::ElementaryStreamDescriptor(
    const ElementaryStreamDescriptor&) = default;
ElementaryStreamDescriptor& ElementaryStreamDescriptor::operator=(
    const ElementaryStreamDescriptor&) = default;
#endif

TrackFragmentRun::TrackFragmentRun() = default;
TrackFragmentRun::~TrackFragmentRun() = default;
TrackFragmentRun::TrackFragmentRun(const TrackFragmentRun&) = default;
TrackFragmentRun& TrackFragmentRun::operator=(const TrackFragmentRun&) =
    default;

MovieFragment::MovieFragment(uint32_t sequence_number)
    : header(sequence_number) {}
MovieFragment::~MovieFragment() = default;
MovieFragment::MovieFragment(const MovieFragment&) = default;
MovieFragment& MovieFragment::operator=(const MovieFragment&) = default;

MediaData::MediaData() = default;
MediaData::~MediaData() = default;
MediaData::MediaData(const MediaData&) = default;
MediaData& MediaData::operator=(const MediaData&) = default;

FileType::FileType(mp4::FourCC in_major_brand, uint32_t in_minor_version)
    : major_brand(in_major_brand), minor_version(in_minor_version) {}
FileType::~FileType() = default;

FragmentRandomAccess::FragmentRandomAccess() = default;
FragmentRandomAccess::~FragmentRandomAccess() = default;

TrackFragmentRandomAccess::TrackFragmentRandomAccess() = default;
TrackFragmentRandomAccess::~TrackFragmentRandomAccess() = default;
TrackFragmentRandomAccess::TrackFragmentRandomAccess(
    const TrackFragmentRandomAccess&) = default;
TrackFragmentRandomAccess& TrackFragmentRandomAccess::operator=(
    const TrackFragmentRandomAccess&) = default;

AudioSampleEntry::AudioSampleEntry(AudioCodec in_codec,
                                   uint32_t in_sample_rate,
                                   uint8_t in_channel_count)
    : codec(in_codec),
      sample_rate(in_sample_rate),
      channel_count(in_channel_count) {}
AudioSampleEntry::~AudioSampleEntry() = default;
AudioSampleEntry::AudioSampleEntry(const AudioSampleEntry&) = default;
AudioSampleEntry& AudioSampleEntry::operator=(const AudioSampleEntry&) =
    default;

VPCodecConfiguration::VPCodecConfiguration(
    VideoCodecProfile in_profile,
    uint8_t in_level,
    const gfx::ColorSpace& in_color_space)
    : profile(in_profile), level(in_level), color_space(in_color_space) {}

VisualSampleEntry::VisualSampleEntry(VideoCodec in_codec) : codec(in_codec) {}
VisualSampleEntry::~VisualSampleEntry() = default;
VisualSampleEntry::VisualSampleEntry(const VisualSampleEntry&) = default;
VisualSampleEntry& VisualSampleEntry::operator=(const VisualSampleEntry&) =
    default;

MediaHandler::MediaHandler(bool is_audio)
    : handler_type(is_audio ? mp4::FOURCC_SOUN : mp4::FOURCC_VIDE) {}

Media::Media(bool is_audio) : handler(is_audio) {}

TrackHeader::TrackHeader(uint32_t in_track_id, bool in_is_audio)
    : track_id(in_track_id), is_audio(in_is_audio) {}

Track::Track(uint32_t track_id, bool is_audio)
    : header(track_id, is_audio), media(is_audio) {}

MovieFragmentHeader::MovieFragmentHeader(uint32_t in_sequence_number)
    : sequence_number(in_sequence_number) {}

OpusSpecificBox::OpusSpecificBox() = default;
OpusSpecificBox::~OpusSpecificBox() = default;
OpusSpecificBox::OpusSpecificBox(const OpusSpecificBox&) = default;
OpusSpecificBox& OpusSpecificBox::operator=(const OpusSpecificBox&) = default;

AV1CodecConfiguration::AV1CodecConfiguration() = default;
AV1CodecConfiguration::~AV1CodecConfiguration() = default;
AV1CodecConfiguration::AV1CodecConfiguration(const AV1CodecConfiguration&) =
    default;
AV1CodecConfiguration& AV1CodecConfiguration::operator=(
    const AV1CodecConfiguration&) = default;

}  // namespace media::mp4::writable_boxes
