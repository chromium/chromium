// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_fragment_box_writer.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "media/formats/mp4/fourccs.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/mp4_type_conversion.h"
#include "media/muxers/output_position_tracker.h"

namespace media {

namespace {

void ValidateSampleFlags(uint32_t flags) {
  uint32_t allowed_sample_flags =
      static_cast<uint32_t>(
          mp4::writable_boxes::FragmentSampleFlags::kSampleFlagIsNonSync) |
      static_cast<uint32_t>(
          mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsYes) |
      static_cast<uint32_t>(
          mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);
  CHECK_EQ(flags & ~(allowed_sample_flags), 0u);
}

}  // namespace

// Mp4MovieFragmentBoxWriter (`moof`) class.
Mp4MovieFragmentBoxWriter::Mp4MovieFragmentBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MovieFragment& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddChildBox(
      std::make_unique<Mp4MovieFragmentHeaderBoxWriter>(context, box_.header));
  for (const auto& fragment : box_.track_fragments) {
    AddChildBox(std::make_unique<Mp4TrackFragmentBoxWriter>(context, fragment));
  }
}

Mp4MovieFragmentBoxWriter::~Mp4MovieFragmentBoxWriter() = default;

void Mp4MovieFragmentBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MOOF);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4MovieFragmentHeaderBoxWriter (`mfhd`) class.
Mp4MovieFragmentHeaderBoxWriter::Mp4MovieFragmentHeaderBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MovieFragmentHeader& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MovieFragmentHeaderBoxWriter::~Mp4MovieFragmentHeaderBoxWriter() = default;

void Mp4MovieFragmentHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_MFHD, /*flags=*/0, /*version=*/0);

  writer.WriteU32(box_.sequence_number);

  writer.EndBox();
}

// Mp4TrackFragmentBoxWriter (`traf`) class.
Mp4TrackFragmentBoxWriter::Mp4TrackFragmentBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragment& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddChildBox(
      std::make_unique<Mp4TrackFragmentHeaderBoxWriter>(context, box_.header));
  AddChildBox(std::make_unique<Mp4TrackFragmentDecodeTimeBoxWriter>(
      context, box_.decode_time));
  AddChildBox(
      std::make_unique<Mp4TrackFragmentRunBoxWriter>(context, box_.run));
}

Mp4TrackFragmentBoxWriter::~Mp4TrackFragmentBoxWriter() = default;

void Mp4TrackFragmentBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_TRAF);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4TrackFragmentHeaderBoxWriter (`tfhd`) class.
Mp4TrackFragmentHeaderBoxWriter::Mp4TrackFragmentHeaderBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragmentHeader& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4TrackFragmentHeaderBoxWriter::~Mp4TrackFragmentHeaderBoxWriter() = default;

void Mp4TrackFragmentHeaderBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_TFHD, box_.flags, /*version=*/0);

  writer.WriteU32(box_.track_id);

  CHECK(box_.flags &
        static_cast<uint32_t>(
            mp4::writable_boxes::TrackFragmentHeaderFlags::kDefaultBaseIsMoof));

  if (box_.flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kSampleDescriptionIndexPresent)) {
    NOTREACHED();
  }

  if (box_.flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kDefaultSampleDurationPresent)) {
    writer.WriteU32(box_.default_sample_duration.InMilliseconds());
  }

  if (box_.flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kDefaultSampleSizePresent)) {
    writer.WriteU32(box_.default_sample_size);
  }

  if (box_.flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kkDefaultSampleFlagsPresent)) {
    ValidateSampleFlags(box_.default_sample_flags);
    writer.WriteU32(box_.default_sample_flags);
  }

  writer.EndBox();
}

// Mp4TrackFragmentDecodeTimeBoxWriter (`tfdt`) class.
Mp4TrackFragmentDecodeTimeBoxWriter::Mp4TrackFragmentDecodeTimeBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragmentDecodeTime& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4TrackFragmentDecodeTimeBoxWriter::~Mp4TrackFragmentDecodeTimeBoxWriter() =
    default;

void Mp4TrackFragmentDecodeTimeBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_TFDT);

  writer.WriteU64(box_.base_media_decode_time.InMilliseconds());

  writer.EndBox();
}

// Mp4TrackFragmentRunBoxWriter (`trun`) class.
Mp4TrackFragmentRunBoxWriter::Mp4TrackFragmentRunBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragmentRun& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4TrackFragmentRunBoxWriter::~Mp4TrackFragmentRunBoxWriter() = default;

void Mp4TrackFragmentRunBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_TRUN, box_.flags, /*version=*/0);

  writer.WriteU32(box_.sample_count);

  {
    // `data_offset`.

    // `movie-fragment relative addressing` should exist by
    // `https://www.w3.org/TR/mse-byte-stream-format-isobmff/`.
    CHECK(box_.flags &
          static_cast<uint16_t>(
              mp4::writable_boxes::TrackFragmentRunFlags::kDataOffsetPresent));
    writer.WriteOffsetPlaceholder();
  }

  if (box_.flags &
      static_cast<uint16_t>(mp4::writable_boxes::TrackFragmentRunFlags::
                                kFirstSampleFlagsPresent)) {
    ValidateSampleFlags(box_.first_sample_flags);
    writer.WriteU32(box_.first_sample_flags);
  }

  bool duration_exists =
      (box_.flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleDurationPresent));
  bool size_exists =
      (box_.flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleSizePresent));
  bool flags_exists =
      (box_.flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleFlagsPresent));

  if (duration_exists) {
    // fragment, if not last, has an additional timestamp entry for last
    // item duration calculation.
    if (box_.sample_count == 0) {
      CHECK_EQ(box_.sample_count, box_.sample_timestamps.size());
    } else {
      CHECK_EQ(box_.sample_count + 1, box_.sample_timestamps.size());
    }
  }

  if (size_exists) {
    CHECK_EQ(box_.sample_count, box_.sample_sizes.size());
  }

  if (flags_exists) {
    CHECK_EQ(box_.sample_count, box_.sample_flags.size());
  }

  for (uint32_t i = 0; i < box_.sample_count; ++i) {
    if (duration_exists) {
      // TODO(crbug.com://1465031): sample_timestamps will be converted to
      // per sample duration with timescale.
      writer.WriteU32(
          (box_.sample_timestamps[i + 1] - box_.sample_timestamps[i])
              .InMilliseconds());
    }

    if (size_exists) {
      writer.WriteU32(box_.sample_sizes[i]);
    }

    if (flags_exists) {
      writer.WriteU32(box_.sample_flags[i]);
    }
  }

  writer.EndBox();
}

// Mp4MediaDataBoxWriter (`mdat`) class.
Mp4MediaDataBoxWriter::Mp4MediaDataBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MediaData& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4MediaDataBoxWriter::~Mp4MediaDataBoxWriter() = default;

void Mp4MediaDataBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MDAT);

  for (const auto& data : box_.track_data) {
    // Write base data offset to the entry of `base_data_offset` of the `trun`,
    // which was set with placeholder during its write.
    writer.FlushCurrentOffset();
    writer.WriteBytes(data.data(), data.size());
  }

  writer.EndBox();
}

}  // namespace media
