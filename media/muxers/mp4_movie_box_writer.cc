// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_movie_box_writer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/big_endian.h"
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
      time.ToDeltaSinceWindowsEpoch().InSeconds() - k1601To1904DeltaInSeconds;

  writer.WriteU64(iso_time);
}

void WriteLowHigh(BoxByteStream& writer, uint32_t value) {
  writer.WriteU16(value & 0xFFFF);
  writer.WriteU16(value >> 16);
}

}  // namespace

// Mp4MovieBoxWriter class.
Mp4MovieBoxWriter::Mp4MovieBoxWriter(const Mp4MuxerContext& input_context,
                                     const mp4::writable_boxes::Movie& box)
    : Mp4BoxWriter(input_context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieHeaderBoxWriter>(context(), box_.header));

  if (auto video_index = context().GetVideoIndex()) {
    DCHECK_LE(*video_index, box_.tracks.size());
    AddChildBox(std::make_unique<Mp4MovieTrackBoxWriter>(
        context(), box_.tracks[*video_index]));
  }

  if (auto audio_index = context().GetAudioIndex()) {
    DCHECK_LE(*audio_index, box_.tracks.size());
    AddChildBox(std::make_unique<Mp4MovieTrackBoxWriter>(
        context(), box_.tracks[*audio_index]));
  }

  AddChildBox(
      std::make_unique<Mp4MovieExtendsBoxWriter>(context(), box_.extends));
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

  writer.StartFullBox(mp4::FOURCC_MVHD);

  WriteIsoTime(writer, box_.creation_time);
  WriteIsoTime(writer, box_.modification_time);
  writer.WriteU32(box_.timescale);
  writer.WriteU64(box_.duration.InSeconds());

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

  writer.WriteU32(box_.next_track_id);

  writer.EndBox();
}

// Mp4MovieExtendsBoxWriter (`mvex`) class.
Mp4MovieExtendsBoxWriter::Mp4MovieExtendsBoxWriter(
    const Mp4MuxerContext& input_context,
    const mp4::writable_boxes::MovieExtends& box)
    : Mp4BoxWriter(input_context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto video_index = context().GetVideoIndex()) {
    DCHECK_LE(*video_index, box_.track_extends.size());
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context(), box_.track_extends[*video_index]));
  }

  if (auto audio_index = context().GetAudioIndex()) {
    DCHECK_LE(*audio_index, box_.track_extends.size());
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context(), box_.track_extends[*audio_index]));
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

  writer.StartFullBox(mp4::FOURCC_TREX);

  writer.WriteU32(box_.track_id);
  writer.WriteU32(box_.default_sample_description_index);
  writer.WriteU32(
      static_cast<uint32_t>(box_.default_sample_duration.InSeconds()));
  writer.WriteU32(box_.default_sample_size);
  writer.WriteU32(box_.default_sample_flags);

  writer.EndBox();
}

// Mp4MovieTrackBoxWriter (`trak`) class.
Mp4MovieTrackBoxWriter::Mp4MovieTrackBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::Track& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieTrackHeaderBoxWriter>(context, box_.header));
  AddChildBox(std::make_unique<Mp4MovieMediaBoxWriter>(context, box_.media));
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

  writer.StartFullBox(mp4::FOURCC_TKHD, box_.flags, /*version*/ 1);

  WriteIsoTime(writer, box_.creation_time);
  WriteIsoTime(writer, box_.modification_time);

  writer.WriteU32(box_.track_id);
  writer.WriteU32(0);  // reserved
  writer.WriteU64(box_.duration.InSeconds());
  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;
  writer.WriteU16(0);  // layer, 0 is the normal value.
  writer.WriteU16(0);  // alternate_group,
  if (box_.is_audio) {
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

  WriteLowHigh(writer, box_.natural_size.width());
  WriteLowHigh(writer, box_.natural_size.height());

  writer.EndBox();
}

// Mp4MovieMediaBoxWriter (`mdia`) class.
Mp4MovieMediaBoxWriter::Mp4MovieMediaBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::Media& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(
      std::make_unique<Mp4MovieMediaHeaderBoxWriter>(context, box_.header));
  AddChildBox(
      std::make_unique<Mp4MovieMediaHandlerBoxWriter>(context, box_.handler));
  AddChildBox(std::make_unique<Mp4MovieMediaInformationBoxWriter>(
      context, box_.information));
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

  writer.StartFullBox(mp4::FOURCC_MDHD);

  WriteIsoTime(writer, box_.creation_time);
  WriteIsoTime(writer, box_.modification_time);

  writer.WriteU32(box_.timescale);
  writer.WriteU64(box_.duration.InSeconds());
  uint16_t language_code = ConvertIso639LanguageCodeToU16(box_.language);
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

  writer.StartFullBox(mp4::FOURCC_HDLR);

  writer.WriteU32(0);  // predefined = 0;
  writer.WriteU32(box_.handler_type);

  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;
  writer.WriteU32(0);  // reserved;

  // zero-terminated C-style string for name.
  if (!box_.name.empty()) {
    writer.WriteBytes(box_.name.c_str(), box_.name.size());
    if (box_.name.back() != 0) {
      writer.WriteU8(0);
    }
  } else {
    writer.WriteU8(0);
  }

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieMediaInformationBoxWriter (`minf`) class.
Mp4MovieMediaInformationBoxWriter::Mp4MovieMediaInformationBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MediaInformation& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieSampleTableBoxWriter>(
      context, box_.sample_table));
}

Mp4MovieMediaInformationBoxWriter::~Mp4MovieMediaInformationBoxWriter() =
    default;

void Mp4MovieMediaInformationBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MINF);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieSampleTableBoxWriter (`stbl`) class.
Mp4MovieSampleTableBoxWriter::Mp4MovieSampleTableBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::SampleTable& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieSampleDescriptionBoxWriter>(
      context, box_.sample_description));
}

Mp4MovieSampleTableBoxWriter::~Mp4MovieSampleTableBoxWriter() = default;

void Mp4MovieSampleTableBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_STBL);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieSampleDescriptionBoxWriter (`stsd`) class.
Mp4MovieSampleDescriptionBoxWriter::Mp4MovieSampleDescriptionBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::SampleDescription& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieSampleDescriptionBoxWriter::~Mp4MovieSampleDescriptionBoxWriter() =
    default;

void Mp4MovieSampleDescriptionBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_STSD);

  writer.WriteU32(box_.entry_count);

  writer.EndBox();
}

}  // namespace media
