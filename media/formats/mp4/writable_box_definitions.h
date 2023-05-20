// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
#define MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/fourccs.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace media::mp4::writable_boxes {

enum class TrackHeaderFlags : uint16_t {
  kTrackEnabled = 0x0001,
  kTrackInMovie = 0x0002,
  kTrackInPreview = 0x0004,
};

// Box header without version.
struct MEDIA_EXPORT Box {};

// Box header with version and flags.
struct MEDIA_EXPORT FullBox : Box {
  // version 1 is 64 bits where applicable, 0 is 32 bits.
  uint8_t version;
  uint32_t flags : 24;
};

// Pixel Aspect Ratio Box (`pasp`) box.
struct MEDIA_EXPORT PixelAspectRatioBox : Box {
  // It has relative width and height of a pixel.
  // We use default value of 1 for both of these values.
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// AVC DecoderConfiguration Record (`avcc`) box.
struct MEDIA_EXPORT AVCDecoderConfiguration : Box {
  // Refer AVCDecoderConfigurationRecord of box_definitions.h
  // because it provides Serialize method and the format
  // is hard to be correct.
  AVCDecoderConfigurationRecord avc_config_record;
};

// VisualSampleEtnry (`avc1`) box.
struct MEDIA_EXPORT VisualSampleEntry : Box {
  gfx::Size coded_size;
  // It is formatted in a fixed 32-byte field, with the first
  // byte set to the number of bytes to be displayed, followed
  // by that number of bytes of displayable data, and then padding
  // to complete 32 bytes total (including the size byte).
  // The field may be set to 0.

  // It will have browser brand name.
  std::string compressor_name;  // char compressor_name[32];
  AVCDecoderConfiguration avc_decoder_configuration;
  PixelAspectRatioBox pixel_aspect_ratio;
};
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// Media sample table (`stsd`) box.
struct MEDIA_EXPORT SampleDescription : FullBox {
  SampleDescription();
  ~SampleDescription();
  SampleDescription(const SampleDescription&);
  SampleDescription& operator=(const SampleDescription&);

  uint32_t entry_count;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  absl::optional<VisualSampleEntry> visual_sample_entry;
#endif
};

// `stco`, `stsz`, `stts`, `stsc`' are mandatory boxes.
// They have 0 child entries in the fragment MP4.

// Media sample table (`stco`) box.
struct MEDIA_EXPORT SampleChunkOffset : FullBox {};

// Media sample table (`stsz`) box.
struct MEDIA_EXPORT SampleSize : FullBox {};

// Decoding Time to Sample (`stts`) box.
struct MEDIA_EXPORT DecodingTimeToSample : FullBox {};

// Media sample table (`stsc`) box.
struct MEDIA_EXPORT SampleToChunk : FullBox {};

// Media sample table (`stbl`) box.
struct MEDIA_EXPORT SampleTable : Box {
  SampleToChunk sample_to_chunk;
  DecodingTimeToSample decoding_time_to_sample;
  SampleSize sample_size;
  SampleChunkOffset sample_chunk_offset;
  SampleDescription sample_description;
};

// Data Url Entry (`url`) box.
struct MEDIA_EXPORT DataUrlEntry : FullBox {};

// Data Reference (`dref`) box.
struct MEDIA_EXPORT DataReference : FullBox {
  DataReference();
  ~DataReference();
  DataReference(const DataReference&);
  DataReference& operator=(const DataReference&);

  std::vector<DataUrlEntry> entries;
};

// Data Information (`dinf`) box.
struct MEDIA_EXPORT DataInformation : Box {
  DataReference data_reference;
};

// Sound Media Information Header (`smdh`) box.
struct MEDIA_EXPORT SoundMediaHeader : FullBox {
  // It has `balance` and `reserved` fields that
  // has `0` as a default value.
};

// Video Media Information Header (`vmhd`) box.
struct MEDIA_EXPORT VideoMediaHeader : FullBox {
  // It has `graphics_mode` and `op_color[3]` that
  // has `0` as a default value.
};

// Media information (`minf`) box.
struct MEDIA_EXPORT MediaInformation : Box {
  absl::optional<VideoMediaHeader> video_header;
  absl::optional<SoundMediaHeader> sound_header;
  DataInformation data_information;
  SampleTable sample_table;
};

// Media Handler (`hdlr`) box.
struct MEDIA_EXPORT MediaHandler : FullBox {
  mp4::FourCC handler_type;
  std::string name;
};

// Media header (`mdhd`) box.
struct MEDIA_EXPORT MediaHeader : FullBox {
  base::Time creation_time;
  base::Time modification_time;
  uint32_t timescale;
  base::TimeDelta duration;
  std::string language;  // 3 letters code ISO-639-2/T language.
};

// Media (`mdia`) box.
struct MEDIA_EXPORT Media : Box {
  MediaHeader header;
  MediaHandler handler;
  MediaInformation information;
};

// Track header (`tkhd`) box.
struct MEDIA_EXPORT TrackHeader : FullBox {
  uint32_t track_id;
  base::Time creation_time;
  base::Time modification_time;
  base::TimeDelta duration;
  bool is_audio;
  gfx::Size natural_size;
};

// Track (`trak`) box.
struct MEDIA_EXPORT Track : Box {
  TrackHeader header;
  Media media;
};

// Track Extends (`trex`) box.
struct MEDIA_EXPORT TrackExtends : FullBox {
  uint32_t track_id;
  uint32_t default_sample_description_index;
  base::TimeDelta default_sample_duration;
  uint32_t default_sample_size;

  // The sample flags field in sample fragments is coded as a 32-bit value.
  // bit(4) reserved=0;
  // unsigned int(2) is_leading;
  // unsigned int(2) sample_depends_on;
  // unsigned int(2) sample_is_depended_on;
  // unsigned int(2) sample_has_redundancy;
  // bit(3) sample_padding_value;
  // bit(1) sample_is_non_sync_sample;
  // unsigned int(16) sample_degradation_priority;
  uint32_t default_sample_flags;
};

// Movie Extends (`mvex`) box.
struct MEDIA_EXPORT MovieExtends : Box {
  MovieExtends();
  ~MovieExtends();
  std::vector<TrackExtends> track_extends;
};

// Movie Header (`mvhd`) box.
struct MEDIA_EXPORT MovieHeader : FullBox {
  MovieHeader();
  ~MovieHeader();

  // It is Windows epoch time so it should be converted to Jan. 1, 1904 UTC
  // before writing. Dates before Jan 1, 1904 UTC will fail / are unsupported.
  base::Time creation_time;
  base::Time modification_time;

  // This is the number of time units that pass in one second.
  uint32_t timescale;
  base::TimeDelta duration;
  uint32_t next_track_id;
};

// Movie (`moov`) box.
struct MEDIA_EXPORT Movie : Box {
  Movie();
  ~Movie();
  MovieHeader header;
  std::vector<Track> tracks;
  MovieExtends extends;
};

}  // namespace media::mp4::writable_boxes

#endif  // MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
