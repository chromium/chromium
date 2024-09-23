// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/muxers/mp4_movie_box_writer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/mp4_type_conversion.h"
#include "media/muxers/output_position_tracker.h"

namespace media {

namespace {

// ISO/IEC 14496-12.
// A transformation matrix for the video.
// Video frames are not scaled, rotated, or skewed, and are displayed at
// their original size with no zoom or depth applied.

// The value 0x00010000 in the top-left and middle element of the
// matrix specifies the horizontal and vertical scaling factor,
// respectively. This means that the video frames are not scaled and
// are displayed at their original size.

// The bottom-right element of the matrix, with a value of 0x40000000,
// specifies the fixed-point value of the zoom or depth of the video frames.
// This value is equal to 1.0 in decimal notation, meaning that there
// is no zoom or depth applied to the video frames.
constexpr int32_t kDisplayIdentityMatrix[9] = {
    0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

void WriteIsoTime(BoxByteStream& writer, base::Time time) {
  uint64_t iso_time =
      base::saturated_cast<uint64_t>((time - kMP4Epoch).InSecondsF());

  writer.WriteU64(iso_time);
}

void WriteLowHigh(BoxByteStream& writer, uint32_t value) {
  writer.WriteU16(value & 0xFFFF);
  writer.WriteU16(value >> 16);
}

}  // namespace

// Mp4FileTypeBoxWriter class.
Mp4FileTypeBoxWriter::Mp4FileTypeBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::FileType& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4FileTypeBoxWriter::~Mp4FileTypeBoxWriter() = default;

void Mp4FileTypeBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_FTYP);

  writer.WriteU32(box_->major_brand);    // normal rate.
  writer.WriteU32(box_->minor_version);  // normal rate.

  // It should include at least of `avc1`.
  CHECK_GE(box_->compatible_brands.size(), 1u);
  CHECK(box_->compatible_brands.end() !=
        std::find(box_->compatible_brands.begin(),
                  box_->compatible_brands.end(), mp4::FOURCC_AVC1));

  for (const uint32_t& brand : box_->compatible_brands) {
    writer.WriteU32(brand);
  }

  writer.EndBox();
}

// Mp4MovieBoxWriter class.
Mp4MovieBoxWriter::Mp4MovieBoxWriter(const Mp4MuxerContext& context,
                                     const mp4::writable_boxes::Movie& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieHeaderBoxWriter>(context, box_->header));

  bool video_is_first_track = false;
  auto video_track = context.GetVideoTrack();
  if (video_track) {
    DCHECK_LE(video_track.value().index, box_->tracks.size());

    video_is_first_track = video_track.value().index == 0;
    if (video_is_first_track) {
      AddChildBox(std::make_unique<Mp4MovieTrackBoxWriter>(
          context, box_->tracks[video_track.value().index]));
    }
  }

  if (auto audio_track = context.GetAudioTrack()) {
    DCHECK_LE(audio_track.value().index, box_->tracks.size());
    AddChildBox(std::make_unique<Mp4MovieTrackBoxWriter>(
        context, box_->tracks[audio_track.value().index]));
  }

  if (!video_is_first_track && video_track) {
    AddChildBox(std::make_unique<Mp4MovieTrackBoxWriter>(
        context, box_->tracks[video_track.value().index]));
  }

  AddChildBox(
      std::make_unique<Mp4MovieExtendsBoxWriter>(context, box_->extends));
}

Mp4MovieBoxWriter::~Mp4MovieBoxWriter() = default;

void Mp4MovieBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MOOV);

  // Write the children.
  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieHeaderBoxWriter class.
Mp4MovieHeaderBoxWriter::Mp4MovieHeaderBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MovieHeader& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieHeaderBoxWriter::~Mp4MovieHeaderBoxWriter() = default;

void Mp4MovieHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_MVHD, /*flags=*/0, /*version=*/1);

  WriteIsoTime(writer, box_->creation_time);
  WriteIsoTime(writer, box_->modification_time);
  writer.WriteU32(box_->timescale);

  // TODO(crbug.com://1465031): The conversion to timescale will be made in
  // the box writer with its duration calculation.
  writer.WriteU64(box_->duration.InMilliseconds());

  writer.WriteU32(0x00010000);  // normal rate.
  writer.WriteU16(0x0100);      // full volume.
  writer.WriteU16(0);           // reserved.
  writer.WriteU32(0);           // reserved.
  writer.WriteU32(0);           // reserved.

  for (auto* it = std::begin(kDisplayIdentityMatrix);
       it != std::end(kDisplayIdentityMatrix); ++it) {
    writer.WriteU32(*it);
  }

  // uint32_t type of predefined[6].
  for (uint32_t i = 0; i < 6; ++i) {
    writer.WriteU32(0);
  }

  writer.WriteU32(box_->next_track_id);

  writer.EndBox();
}

// Mp4MovieExtendsBoxWriter (`mvex`) class.
Mp4MovieExtendsBoxWriter::Mp4MovieExtendsBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MovieExtends& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto video_track = context.GetVideoTrack()) {
    DCHECK_LE(video_track.value().index, box_->track_extends.size());
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context, box_->track_extends[video_track.value().index]));
  }

  if (auto audio_track = context.GetAudioTrack()) {
    DCHECK_LE(audio_track.value().index, box_->track_extends.size());
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context, box_->track_extends[audio_track.value().index]));
  }
}

Mp4MovieExtendsBoxWriter::~Mp4MovieExtendsBoxWriter() = default;

void Mp4MovieExtendsBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MVEX);

  // Write the children.
  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieTrackExtendsBoxWriter (`trex`) class.
Mp4MovieTrackExtendsBoxWriter::Mp4MovieTrackExtendsBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackExtends& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieTrackExtendsBoxWriter::~Mp4MovieTrackExtendsBoxWriter() = default;

void Mp4MovieTrackExtendsBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_TREX, /*flags=*/0, /*version=*/0);

  writer.WriteU32(box_->track_id);
  writer.WriteU32(box_->default_sample_description_index);
  writer.WriteU32(
      static_cast<uint32_t>(box_->default_sample_duration.InMilliseconds()));
  writer.WriteU32(box_->default_sample_size);
  writer.WriteU32(box_->default_sample_flags);

  writer.EndBox();
}

// Mp4MovieTrackBoxWriter (`trak`) class.
Mp4MovieTrackBoxWriter::Mp4MovieTrackBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::Track& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieTrackHeaderBoxWriter>(context, box_->header));
  AddChildBox(std::make_unique<Mp4MovieMediaBoxWriter>(context, box_->media));
}

Mp4MovieTrackBoxWriter::~Mp4MovieTrackBoxWriter() = default;

void Mp4MovieTrackBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_TRAK);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieTrackHeaderBoxWriter (`tkhd`) class.
Mp4MovieTrackHeaderBoxWriter::Mp4MovieTrackHeaderBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackHeader& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieTrackHeaderBoxWriter::~Mp4MovieTrackHeaderBoxWriter() = default;

void Mp4MovieTrackHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_TKHD, box_->flags, /*version=*/1);

  WriteIsoTime(writer, box_->creation_time);
  WriteIsoTime(writer, box_->modification_time);

  writer.WriteU32(box_->track_id);
  writer.WriteU32(0);  // reserved
  writer.WriteU64(box_->duration.InMilliseconds());
  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;
  writer.WriteU16(0);  // layer, 0 is the normal value.
  writer.WriteU16(0);  // alternate_group,
  if (box_->is_audio) {
    // 1.0 (0x0100) is a full volume for the audio.
    writer.WriteU16(0x0100);
  } else {
    writer.WriteU16(0);
  }
  writer.WriteU16(0);  // reserved.

  for (auto* it = std::begin(kDisplayIdentityMatrix);
       it != std::end(kDisplayIdentityMatrix); ++it) {
    writer.WriteU32(*it);
  }

  WriteLowHigh(writer, box_->natural_size.width());
  WriteLowHigh(writer, box_->natural_size.height());

  writer.EndBox();
}

// Mp4MovieMediaBoxWriter (`mdia`) class.
Mp4MovieMediaBoxWriter::Mp4MovieMediaBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::Media& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieMediaHeaderBoxWriter>(context, box_->header));
  AddChildBox(
      std::make_unique<Mp4MovieMediaHandlerBoxWriter>(context, box_->handler));
  AddChildBox(std::make_unique<Mp4MovieMediaInformationBoxWriter>(
      context, box_->information));
}

Mp4MovieMediaBoxWriter::~Mp4MovieMediaBoxWriter() = default;

void Mp4MovieMediaBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MDIA);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieMediaHeaderBoxWriter (`mdhd`) class.
Mp4MovieMediaHeaderBoxWriter::Mp4MovieMediaHeaderBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MediaHeader& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieMediaHeaderBoxWriter::~Mp4MovieMediaHeaderBoxWriter() = default;

void Mp4MovieMediaHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_MDHD, /*flags=*/0, /*version=*/1);

  WriteIsoTime(writer, box_->creation_time);
  WriteIsoTime(writer, box_->modification_time);

  writer.WriteU32(box_->timescale);
  writer.WriteU64(box_->duration.InMilliseconds());
  uint16_t language_code = ConvertIso639LanguageCodeToU16(box_->language);
  writer.WriteU16(language_code);
  writer.WriteU16(0);  // pre_defined = 0;

  writer.EndBox();
}

// Mp4MovieMediaHandlerBoxWriter (`hdlr`) class.
Mp4MovieMediaHandlerBoxWriter::Mp4MovieMediaHandlerBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MediaHandler& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieMediaHandlerBoxWriter::~Mp4MovieMediaHandlerBoxWriter() = default;

void Mp4MovieMediaHandlerBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_HDLR, /*flags=*/0);

  writer.WriteU32(0);  // predefined = 0;
  writer.WriteU32(box_->handler_type);

  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;

  writer.WriteString(box_->name);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieMediaInformationBoxWriter (`minf`) class.
Mp4MovieMediaInformationBoxWriter::Mp4MovieMediaInformationBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MediaInformation& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (box_->video_header.has_value()) {
    AddChildBox(std::make_unique<Mp4MovieVideoHeaderBoxWriter>(context));
  } else if (box_->sound_header.has_value()) {
    AddChildBox(std::make_unique<Mp4MovieSoundHeaderBoxWriter>(context));
  }

  AddChildBox(std::make_unique<Mp4MovieDataInformationBoxWriter>(
      context, box_->data_information));

  AddChildBox(std::make_unique<Mp4MovieSampleTableBoxWriter>(
      context, box_->sample_table));
}

Mp4MovieMediaInformationBoxWriter::~Mp4MovieMediaInformationBoxWriter() =
    default;

void Mp4MovieMediaInformationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MINF);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieVideoHeaderBoxWriter (`vmhd`) class.
Mp4MovieVideoHeaderBoxWriter::Mp4MovieVideoHeaderBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieVideoHeaderBoxWriter::~Mp4MovieVideoHeaderBoxWriter() = default;

void Mp4MovieVideoHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_VMHD, /*flags=*/1);

  writer.WriteU16(0);  // graphics_mode.
  writer.WriteU16(0);  // op_color[0].
  writer.WriteU16(0);  // op_color[1]..
  writer.WriteU16(0);  // op_color[2]..

  writer.EndBox();
}

// Mp4MovieSoundHeaderBoxWriter (`smhd`) class.
Mp4MovieSoundHeaderBoxWriter::Mp4MovieSoundHeaderBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieSoundHeaderBoxWriter::~Mp4MovieSoundHeaderBoxWriter() = default;

void Mp4MovieSoundHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_SMHD, /*flags=*/0);

  writer.WriteU16(0);  // balance.
  writer.WriteU16(0);  // reserved.

  writer.EndBox();
}

// Mp4MovieDataInformationBoxWriter (`dinf`) class.
Mp4MovieDataInformationBoxWriter::Mp4MovieDataInformationBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::DataInformation& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieDataReferenceBoxWriter>(
      context, box_->data_reference));
}

Mp4MovieDataInformationBoxWriter::~Mp4MovieDataInformationBoxWriter() = default;

void Mp4MovieDataInformationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_DINF);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieDataReferenceBoxWriter (`dref`) class.
Mp4MovieDataReferenceBoxWriter::Mp4MovieDataReferenceBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::DataReference& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < box_->entries.size(); ++i) {
    AddChildBox(std::make_unique<Mp4MovieDataUrlEntryBoxWriter>(context));
  }
}

Mp4MovieDataReferenceBoxWriter::~Mp4MovieDataReferenceBoxWriter() = default;

void Mp4MovieDataReferenceBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_DREF, /*flags=*/0);

  writer.WriteU32(box_->entries.size());

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieDataUrlEntryBoxWriter (`url`) class.
Mp4MovieDataUrlEntryBoxWriter::Mp4MovieDataUrlEntryBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieDataUrlEntryBoxWriter::~Mp4MovieDataUrlEntryBoxWriter() = default;

void Mp4MovieDataUrlEntryBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_URL, /*flags=*/1);

  // We use empty Url location to prevent accidental PII leak.
  writer.WriteString("");

  writer.EndBox();
}

// Mp4MovieSampleTableBoxWriter (`stbl`) class.
Mp4MovieSampleTableBoxWriter::Mp4MovieSampleTableBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::SampleTable& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieSampleToChunkBoxWriter>(context));
  AddChildBox(std::make_unique<Mp4MovieDecodingTimeToSampleBoxWriter>(context));
  AddChildBox(std::make_unique<Mp4MovieSampleSizeBoxWriter>(context));
  AddChildBox(std::make_unique<Mp4MovieSampleChunkOffsetBoxWriter>(context));
  AddChildBox(std::make_unique<Mp4MovieSampleDescriptionBoxWriter>(
      context, box_->sample_description));
}

Mp4MovieSampleTableBoxWriter::~Mp4MovieSampleTableBoxWriter() = default;

void Mp4MovieSampleTableBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_STBL);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieSampleToChunkBoxWriter (`stsc`) class.
Mp4MovieSampleToChunkBoxWriter::Mp4MovieSampleToChunkBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieSampleToChunkBoxWriter::~Mp4MovieSampleToChunkBoxWriter() = default;

void Mp4MovieSampleToChunkBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STSC, /*flags=*/0);

  writer.WriteU32(0);  // entry_count.

  writer.EndBox();
}

// Mp4MovieDecodingTimeToSampleBoxWriter (`stts`) class.
Mp4MovieDecodingTimeToSampleBoxWriter::Mp4MovieDecodingTimeToSampleBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieDecodingTimeToSampleBoxWriter::
    ~Mp4MovieDecodingTimeToSampleBoxWriter() = default;

void Mp4MovieDecodingTimeToSampleBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STTS, /*flags=*/0);

  writer.WriteU32(0);  // entry_count.

  writer.EndBox();
}

// Mp4MovieSampleSizeBoxWriter (`stsz`) class.
Mp4MovieSampleSizeBoxWriter::Mp4MovieSampleSizeBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieSampleSizeBoxWriter::~Mp4MovieSampleSizeBoxWriter() = default;

void Mp4MovieSampleSizeBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STSZ);

  writer.WriteU32(0);  // sample_size.
  writer.WriteU32(0);  // sample_count.

  writer.EndBox();
}

// Mp4MovieSampleChunkOffsetBoxWriter (`stco`) class.
Mp4MovieSampleChunkOffsetBoxWriter::Mp4MovieSampleChunkOffsetBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieSampleChunkOffsetBoxWriter::~Mp4MovieSampleChunkOffsetBoxWriter() =
    default;

void Mp4MovieSampleChunkOffsetBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STCO, /*flags=*/0);

  writer.WriteU32(0);  // entry_count.

  writer.EndBox();
}

// Mp4MovieSampleDescriptionBoxWriter (`stsd`) class.
Mp4MovieSampleDescriptionBoxWriter::Mp4MovieSampleDescriptionBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::SampleDescription& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (box_->video_sample_entry.has_value()) {
    CHECK(!box_->audio_sample_entry.has_value());
    AddChildBox(std::make_unique<Mp4MovieVisualSampleEntryBoxWriter>(
        context, box_->video_sample_entry.value()));
    return;
  }

  CHECK(box_->audio_sample_entry.has_value());
  AddChildBox(std::make_unique<Mp4MovieAudioSampleEntryBoxWriter>(
      context, box_->audio_sample_entry.value()));
}

Mp4MovieSampleDescriptionBoxWriter::~Mp4MovieSampleDescriptionBoxWriter() =
    default;

void Mp4MovieSampleDescriptionBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STSD, /*flags=*/0);

  writer.WriteU32(box_->entry_count);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieVisualSampleEntryBoxWriter (`avc1`, 'vp09') class.
Mp4MovieVisualSampleEntryBoxWriter::Mp4MovieVisualSampleEntryBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::VisualSampleEntry& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddChildBox(std::make_unique<Mp4MoviePixelAspectRatioBoxBoxWriter>(context));
  AddChildBox(
      std::make_unique<Mp4MovieBitRateBoxWriter>(context, box_->bit_rate));

  switch (box_->codec) {
    case VideoCodec::kVP9:
      CHECK(box_->vp_decoder_configuration.has_value());
      AddChildBox(std::make_unique<Mp4MovieVPCodecConfigurationBoxWriter>(
          context, box_->vp_decoder_configuration.value()));
      break;
    case VideoCodec::kAV1:
      CHECK(box_->av1_decoder_configuration.has_value());
      AddChildBox(std::make_unique<Mp4MovieAV1CodecConfigurationBoxWriter>(
          context, box_->av1_decoder_configuration.value()));
      break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kH264:
      CHECK(box_->avc_decoder_configuration.has_value());
      AddChildBox(std::make_unique<Mp4MovieAVCDecoderConfigurationBoxWriter>(
          context, box_->avc_decoder_configuration.value()));
      break;
#endif
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

Mp4MovieVisualSampleEntryBoxWriter::~Mp4MovieVisualSampleEntryBoxWriter() =
    default;

void Mp4MovieVisualSampleEntryBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (box_->codec) {
    case VideoCodec::kVP9:
      writer.StartBox(mp4::FOURCC_VP09);
      break;
    case VideoCodec::kAV1:
      writer.StartBox(mp4::FOURCC_AV01);
      break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kH264:
      writer.StartBox(mp4::FOURCC_AVC1);
      break;
#endif
    default:
      NOTREACHED_IN_MIGRATION();
  }

  writer.WriteU32(0);  // reserved.
  writer.WriteU16(0);  // reserved.
  writer.WriteU16(1);  // data_reference_index in `dref` box, 1 is start index.
  writer.WriteU16(0);  // predefined1.
  writer.WriteU16(1);  // reserved2.
  writer.WriteU32(0);  // pre_defined2[0].
  writer.WriteU32(0);  // pre_defined2[1].
  writer.WriteU32(0);  // pre_defined2[2].
  writer.WriteU16(box_->coded_size.width());
  writer.WriteU16(box_->coded_size.height());
  writer.WriteU32(0x00480000);  // horizontal resolution, 72 dpi.
  writer.WriteU32(0x00480000);  // vertical resolution, 72 dpi.
  writer.WriteU32(0);           // reserved3.
  writer.WriteU16(1);           // frame_count.

  // compressor_name.
  constexpr size_t kMaxCompressorNameSize = 30;

  std::string compressor_name = box_->compressor_name;
  uint8_t compressor_name_size =
      std::min(compressor_name.size(), kMaxCompressorNameSize);
  writer.WriteU8(compressor_name_size);
  compressor_name.resize(kMaxCompressorNameSize);
  compressor_name.push_back(0);
  writer.WriteString(compressor_name);  // It will write 31 chars.

  writer.WriteU16(0x0018);  // depth.
  writer.WriteU16(0xFFFF);  // pre_defined, -1.

  WriteChildren(writer);

  writer.EndBox();
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Mp4MovieAVCDecoderConfigurationBoxWriter (`avcC`) class.
Mp4MovieAVCDecoderConfigurationBoxWriter::
    Mp4MovieAVCDecoderConfigurationBoxWriter(
        const Mp4MuxerContext& context,
        const mp4::writable_boxes::AVCDecoderConfiguration& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieAVCDecoderConfigurationBoxWriter::
    ~Mp4MovieAVCDecoderConfigurationBoxWriter() = default;

void Mp4MovieAVCDecoderConfigurationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_AVCC);

  std::vector<uint8_t> write_data;
  CHECK(box_->avc_config_record.Serialize(write_data));

  writer.WriteBytes(write_data.data(), write_data.size());

  writer.EndBox();
}

// Mp4MovieElementaryStreamDescriptorBoxWriter (`esds`) class.
Mp4MovieElementaryStreamDescriptorBoxWriter::
    Mp4MovieElementaryStreamDescriptorBoxWriter(
        const Mp4MuxerContext& context,
        const mp4::writable_boxes::ElementaryStreamDescriptor& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieElementaryStreamDescriptorBoxWriter::
    ~Mp4MovieElementaryStreamDescriptorBoxWriter() = default;

void Mp4MovieElementaryStreamDescriptorBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_ESDS);

  std::vector<uint8_t> esds =
      mp4::ESDescriptor::CreateEsds(box_->aac_codec_description);
  writer.WriteBytes(esds.data(), esds.size());

  writer.EndBox();
}
#endif

// Mp4MovieAudioSampleEntryBoxWriter (`mp4a` or 'Opus') class.
Mp4MovieAudioSampleEntryBoxWriter::Mp4MovieAudioSampleEntryBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::AudioSampleEntry& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieBitRateBoxWriter>(context, box_->bit_rate));

  switch (box_->codec) {
    case AudioCodec::kOpus:
      AddChildBox(std::make_unique<Mp4MovieOpusSpecificBoxWriter>(
          context, box_->opus_specific_box.value()));
      break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case AudioCodec::kAAC:
      AddChildBox(std::make_unique<Mp4MovieElementaryStreamDescriptorBoxWriter>(
          context, box_->elementary_stream_descriptor.value()));
      break;
#endif
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

Mp4MovieAudioSampleEntryBoxWriter::~Mp4MovieAudioSampleEntryBoxWriter() =
    default;

void Mp4MovieAudioSampleEntryBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (box_->codec) {
    case AudioCodec::kOpus:
      writer.StartBox(mp4::FOURCC_OPUS);
      break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case AudioCodec::kAAC:
      writer.StartBox(mp4::FOURCC_MP4A);
      break;
#endif
    default:
      NOTREACHED_IN_MIGRATION();
  }

  constexpr size_t kAudioSampleEntryReservedSize = 6u;
  for (size_t i = 0; i < kAudioSampleEntryReservedSize; ++i) {
    writer.WriteU8(0);
  }

  writer.WriteU16(1);  // data_reference_index in `dref` box, 1 is start index.
  writer.WriteU32(0);  // reserved1[0]
  writer.WriteU32(0);  // reserved1[1]
  writer.WriteU16(box_->channel_count);
  writer.WriteU16(16);  // sample size.
  writer.WriteU16(0);   // predefined.
  writer.WriteU16(0);   // reserved.

  WriteLowHigh(writer, box_->sample_rate);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieOpusSpecificBoxWriter (`dOps`) class.
Mp4MovieOpusSpecificBoxWriter::Mp4MovieOpusSpecificBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::OpusSpecificBox& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieOpusSpecificBoxWriter::~Mp4MovieOpusSpecificBoxWriter() = default;

void Mp4MovieOpusSpecificBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_DOPS);

  constexpr base::TimeDelta kOpusSkipDuration = base::Milliseconds(80);
  writer.WriteU8(0u);  // Version.
  writer.WriteU8(box_->channel_count);
  writer.WriteU16(static_cast<uint16_t>(AudioTimestampHelper::TimeToFrames(
      kOpusSkipDuration, box_->sample_rate)));  // Preskip.
  writer.WriteU32(box_->sample_rate);
  writer.WriteU16(0u);  // OutputGain.
  writer.WriteU8(0u);   // ChannelMappingFamily

  // TODO(crbug.com/330815378): Write channel mapping table.
  writer.EndBox();
}

// Mp4MovieVPCodecConfigurationBoxWriter (`vpcC`) class.
Mp4MovieVPCodecConfigurationBoxWriter::Mp4MovieVPCodecConfigurationBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::VPCodecConfiguration& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieVPCodecConfigurationBoxWriter::
    ~Mp4MovieVPCodecConfigurationBoxWriter() = default;

void Mp4MovieVPCodecConfigurationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_VPCC);

  switch (box_->profile) {
    case VP9PROFILE_PROFILE0:
      writer.WriteU8(0);
      break;
    case VP9PROFILE_PROFILE1:
      writer.WriteU8(1);
      break;
    case VP9PROFILE_PROFILE2:
      writer.WriteU8(2);
      break;
    case VP9PROFILE_PROFILE3:
      writer.WriteU8(3);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  writer.WriteU8(box_->level);

  auto video_color_space =
      box_->color_space.IsValid()
          ? VideoColorSpace::FromGfxColorSpace(box_->color_space)
          : VideoColorSpace(VideoColorSpace::PrimaryID::UNSPECIFIED,
                            VideoColorSpace::TransferID::UNSPECIFIED,
                            VideoColorSpace::MatrixID::UNSPECIFIED,
                            gfx::ColorSpace::RangeID::DERIVED);

  constexpr uint8_t bit_depth = 8u;
  constexpr uint8_t chroma_sub_sampling = 0u;
  uint8_t video_full_range_flag =
      video_color_space.range == gfx::ColorSpace::RangeID::FULL ? 1u : 0u;

  writer.WriteU8(bit_depth << 4 | chroma_sub_sampling << 1 |
                 video_full_range_flag);
  writer.WriteU8(static_cast<uint8_t>(video_color_space.primaries));
  writer.WriteU8(static_cast<uint8_t>(video_color_space.transfer));
  writer.WriteU8(static_cast<uint8_t>(video_color_space.matrix));
  writer.WriteU16(/*codecInitializationData Size*/ 0);

  writer.EndBox();
}

// Mp4MovieAV1CodecConfigurationBoxWriter (`vpcC`) class.
Mp4MovieAV1CodecConfigurationBoxWriter::Mp4MovieAV1CodecConfigurationBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::AV1CodecConfiguration& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieAV1CodecConfigurationBoxWriter::
    ~Mp4MovieAV1CodecConfigurationBoxWriter() = default;

void Mp4MovieAV1CodecConfigurationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_AV1C);
  writer.WriteBytes(box_->av1_decoder_configuration_data.data(),
                    box_->av1_decoder_configuration_data.size());
  writer.EndBox();
}

// Mp4MoviePixelAspectRatioBoxBoxWriter (`pasp`) class.
Mp4MoviePixelAspectRatioBoxBoxWriter::Mp4MoviePixelAspectRatioBoxBoxWriter(
    const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MoviePixelAspectRatioBoxBoxWriter::~Mp4MoviePixelAspectRatioBoxBoxWriter() =
    default;

void Mp4MoviePixelAspectRatioBoxBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_PASP);

  writer.WriteU32(1);  // Horizontal spacing.
  writer.WriteU32(1);  // Vertical spacing.

  writer.EndBox();
}

// Mp4MovieBitRateBoxWriter (`btrt`) class.
Mp4MovieBitRateBoxWriter::Mp4MovieBitRateBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::BitRate& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieBitRateBoxWriter::~Mp4MovieBitRateBoxWriter() = default;

void Mp4MovieBitRateBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_BTRT);

  // bufferSizeDB, 0 for unknown so that decoder will decide with its own
  // algorithm.
  writer.WriteU32(0);

  writer.WriteU32(box_->max_bit_rate);
  writer.WriteU32(box_->avg_bit_rate);

  writer.EndBox();
}

}  // namespace media
