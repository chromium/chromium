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
#include "media/muxers/output_position_tracker.h"

namespace media {

namespace {}  // namespace

// Mp4MovieBoxWriter class.
Mp4MovieBoxWriter::Mp4MovieBoxWriter(const Mp4MuxerContext& context,
                                     const mp4::writable_boxes::Movie& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddChildBox(std::make_unique<Mp4MovieHeaderBoxWriter>(context, box_.header));
  AddChildBox(
      std::make_unique<Mp4MovieExtendsBoxWriter>(context, box_.extends));
}

Mp4MovieBoxWriter::~Mp4MovieBoxWriter() = default;

void Mp4MovieBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox();

  CHECK_EQ(box_.fourcc, mp4::FOURCC_MOOV);
  WriteBox(writer, box_.fourcc);

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

  writer.StartBox();

  CHECK_EQ(box_.fourcc, mp4::FOURCC_MVHD);
  WriteFullBox(writer, box_.fourcc);

  // Convert to ISO time, seconds since midnight, Jan. 1, 1904, in UTC time.
  // base::Time time1904;
  // base::Time::FromUTCString("1904-01-01 00:00:00 UTC", &time1904);
  // 9561628800 = time1904.ToDeltaSinceWindowsEpoch().InSeconds();
  constexpr int64_t k1601To1904DeltaInSeconds = INT64_C(9561628800);

  uint64_t iso_creation_time =
      box_.creation_time.ToDeltaSinceWindowsEpoch().InSeconds() -
      k1601To1904DeltaInSeconds;
  uint64_t iso_modification_time =
      box_.modification_time.ToDeltaSinceWindowsEpoch().InSeconds() -
      k1601To1904DeltaInSeconds;

  writer.WriteU64(iso_creation_time);
  writer.WriteU64(iso_modification_time);
  writer.WriteU32(box_.timescale);
  writer.WriteU64(box_.duration.InSeconds());

  writer.WriteU32(0x00010000);  // normal rate.
  writer.WriteU16(0x0100);      // full volume.
  writer.WriteU16(0);           // reserved.
  writer.WriteU32(0);           // reserved.
  writer.WriteU32(0);           // reserved.

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

  if (context().GetVideoIndex().has_value()) {
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context(), box_.track_extends[context().GetVideoIndex().value()]));
  }

  if (context().GetAudioIndex().has_value()) {
    AddChildBox(std::make_unique<Mp4MovieTrackExtendsBoxWriter>(
        context(), box_.track_extends[context().GetAudioIndex().value()]));
  }
}

Mp4MovieExtendsBoxWriter::~Mp4MovieExtendsBoxWriter() = default;

void Mp4MovieExtendsBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox();

  CHECK_EQ(box_.fourcc, mp4::FOURCC_MVEX);
  WriteBox(writer, box_.fourcc);

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

  writer.StartBox();

  CHECK_EQ(box_.fourcc, mp4::FOURCC_TREX);
  WriteFullBox(writer, box_.fourcc);

  writer.WriteU32(box_.track_id);
  writer.WriteU32(box_.default_sample_description_index);
  writer.WriteU32(
      static_cast<uint32_t>(box_.default_sample_duration.InSeconds()));
  writer.WriteU32(box_.default_sample_size);
  writer.WriteU32(box_.default_sample_flags);

  writer.EndBox();
}

}  // namespace media
