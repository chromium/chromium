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

bool IsVideoTrackBox(const Mp4MuxerContext& context, uint32_t track_id) {
  // The Mp4MuxerDelegate sets the track id with plus 1 over track index,
  // which is 0 based on the internal fragments list.
  if (auto video_track = context.GetVideoTrack()) {
    return (video_track.value().index + 1) == track_id;
  }
  return false;
}

}  // namespace

// Mp4MovieFragmentBoxWriter (`moof`) class.
Mp4MovieFragmentBoxWriter::Mp4MovieFragmentBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::MovieFragment& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddChildBox(
      std::make_unique<Mp4MovieFragmentHeaderBoxWriter>(context, box_->header));
  for (const auto& fragment : box_->track_fragments) {
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

  writer.StartFullBox(mp4::FOURCC_MFHD, /*flags=*/0);

  writer.WriteU32(box_->sequence_number);

  writer.EndBox();
}

// Mp4TrackFragmentBoxWriter (`traf`) class.
Mp4TrackFragmentBoxWriter::Mp4TrackFragmentBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragment& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddChildBox(
      std::make_unique<Mp4TrackFragmentHeaderBoxWriter>(context, box_->header));
  AddChildBox(std::make_unique<Mp4TrackFragmentDecodeTimeBoxWriter>(
      context, box_->decode_time));
  AddChildBox(
      std::make_unique<Mp4TrackFragmentRunBoxWriter>(context, box_->run));
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

  writer.StartFullBox(mp4::FOURCC_TFHD, box_->flags);

  writer.WriteU32(box_->track_id);

  CHECK(box_->flags &
        static_cast<uint32_t>(
            mp4::writable_boxes::TrackFragmentHeaderFlags::kDefaultBaseIsMoof));

  if (box_->flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kSampleDescriptionIndexPresent)) {
    NOTREACHED_IN_MIGRATION();
  }

  if (box_->flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kDefaultSampleDurationPresent)) {
    writer.WriteU32(box_->default_sample_duration.InMilliseconds());
  }

  if (box_->flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kDefaultSampleSizePresent)) {
    writer.WriteU32(box_->default_sample_size);
  }

  if (box_->flags &
      static_cast<uint32_t>(mp4::writable_boxes::TrackFragmentHeaderFlags::
                                kkDefaultSampleFlagsPresent)) {
    ValidateSampleFlags(box_->default_sample_flags);
    writer.WriteU32(box_->default_sample_flags);
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

  writer.StartFullBox(mp4::FOURCC_TFDT, /*flags=*/0, /*version=*/1);

  uint32_t timescale = 0;
  if (IsVideoTrackBox(context(), box_->track_id)) {
    timescale = context().GetVideoTrack().value().timescale;
  } else {
    timescale = context().GetAudioTrack().value().timescale;
  }

  writer.WriteU64(ConvertToTimescale(box_->base_media_decode_time, timescale));

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

  writer.StartFullBox(mp4::FOURCC_TRUN, box_->flags, /*version=*/1);

  writer.WriteU32(box_->sample_count);

  {
    // `data_offset`.

    // `movie-fragment relative addressing` should exist by
    // `https://www.w3.org/TR/mse-byte-stream-format-isobmff/`.
    CHECK(box_->flags &
          static_cast<uint16_t>(
              mp4::writable_boxes::TrackFragmentRunFlags::kDataOffsetPresent));
    writer.WriteOffsetPlaceholder();
  }

  uint32_t timescale = 0;
  if (box_->flags &
      static_cast<uint16_t>(mp4::writable_boxes::TrackFragmentRunFlags::
                                kFirstSampleFlagsPresent)) {
    ValidateSampleFlags(box_->first_sample_flags);
    writer.WriteU32(box_->first_sample_flags);

    // `kFirstSampleFlagsPresent` exists for only Video track.
    timescale = context().GetVideoTrack().value().timescale;
  } else {
    timescale = context().GetAudioTrack().value().timescale;
  }

  bool duration_exists =
      (box_->flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleDurationPresent));
  bool size_exists =
      (box_->flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleSizePresent));
  bool flags_exists =
      (box_->flags &
       static_cast<uint16_t>(
           mp4::writable_boxes::TrackFragmentRunFlags::kSampleFlagsPresent));

  if (duration_exists) {
    // fragment, if not last, has an additional timestamp entry for last
    // item duration calculation.
    if (box_->sample_count == 0) {
      CHECK_EQ(box_->sample_count, box_->sample_timestamps.size());
    } else {
      CHECK_EQ(box_->sample_count + 1, box_->sample_timestamps.size());
    }
  }

  if (size_exists) {
    CHECK_EQ(box_->sample_count, box_->sample_sizes.size());
  }

  if (flags_exists) {
    CHECK_EQ(box_->sample_count, box_->sample_flags.size());
  }

  for (uint32_t i = 0; i < box_->sample_count; ++i) {
    if (duration_exists) {
      base::TimeDelta time_diff =
          box_->sample_timestamps[i + 1] - box_->sample_timestamps[i];
      writer.WriteU32(
          static_cast<uint32_t>(ConvertToTimescale(time_diff, timescale)));
    }

    if (size_exists) {
      writer.WriteU32(box_->sample_sizes[i]);
    }

    if (flags_exists) {
      writer.WriteU32(box_->sample_flags[i]);
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

  for (const auto& data : box_->track_data) {
    // Write base data offset to the entry of `base_data_offset` of the `trun`,
    // which was set with placeholder during its write.
    writer.FlushCurrentOffset();
    writer.WriteBytes(data.data(), data.size());
  }

  writer.EndBox();
}

// Mp4FragmentRandomAccessBoxWriter (`mfra`) class.
Mp4FragmentRandomAccessBoxWriter::Mp4FragmentRandomAccessBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::FragmentRandomAccess& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It will add video track only because every sample are sync samples in the
  // audio.
  if (auto video_track = context.GetVideoTrack()) {
    AddChildBox(std::make_unique<Mp4TrackFragmentRandomAccessBoxWriter>(
        context, box_->tracks[video_track.value().index]));
  }

  AddChildBox(
      std::make_unique<Mp4FragmentRandomAccessOffsetBoxBoxWriter>(context));
}

Mp4FragmentRandomAccessBoxWriter::~Mp4FragmentRandomAccessBoxWriter() = default;

void Mp4FragmentRandomAccessBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartBox(mp4::FOURCC_MFRA);

  WriteChildren(writer);

  writer.EndBox();
}

// Mp4TrackFragmentRandomAccessBoxWriter (`tfra`) class.
Mp4TrackFragmentRandomAccessBoxWriter::Mp4TrackFragmentRandomAccessBoxWriter(
    const Mp4MuxerContext& context,
    const mp4::writable_boxes::TrackFragmentRandomAccess& box)
    : Mp4BoxWriter(context), box_(box) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4TrackFragmentRandomAccessBoxWriter::
    ~Mp4TrackFragmentRandomAccessBoxWriter() = default;

void Mp4TrackFragmentRandomAccessBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_TFRA, /*flags=*/0, /*version=*/1);

  // `length_size_of_traf_num`, `length_size_of_trun_num` and
  // `length_size_of_sample_num` size is 4 (uint32_t size).
  constexpr uint32_t kLengthSizeOfEntries = 0x0000003F;

  writer.WriteU32(box_->track_id);
  writer.WriteU32(kLengthSizeOfEntries);
  writer.WriteU32(box_->entries.size());

  for (const mp4::writable_boxes::TrackFragmentRandomAccessEntry& entry :
       box_->entries) {
    // TODO(crbug.com://1471314): convert the presentation time based on
    // the track's timescale.
    uint32_t timescale = context().GetVideoTrack().value().timescale;
    writer.WriteU64(ConvertToTimescale(entry.time, timescale));
    writer.WriteU64(entry.moof_offset);
    writer.WriteU32(entry.traf_number);
    writer.WriteU32(entry.trun_number);
    writer.WriteU32(entry.sample_number);
  }

  writer.EndBox();
}

// Mp4FragmentRandomAccessOffsetBoxBoxWriter (`mfro`) class.
Mp4FragmentRandomAccessOffsetBoxBoxWriter::
    Mp4FragmentRandomAccessOffsetBoxBoxWriter(const Mp4MuxerContext& context)
    : Mp4BoxWriter(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Mp4FragmentRandomAccessOffsetBoxBoxWriter::
    ~Mp4FragmentRandomAccessOffsetBoxBoxWriter() = default;

void Mp4FragmentRandomAccessOffsetBoxBoxWriter::Write(BoxByteStream& writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  writer.StartFullBox(mp4::FOURCC_MFRO, /*flags=*/0, /*version=*/1);

  // `size` property of the `mfro` box is the total size of the `mfra` box.
  writer.WriteU32(writer.size() + 4);

  writer.EndBox();
}

}  // namespace media
