// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
#define MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
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

enum class TrackFragmentHeaderFlags : uint32_t {
  kBaseDataOffsetPresent = 0x0001,
  kSampleDescriptionIndexPresent = 0x0002,
  kDefaultSampleDurationPresent = 0x0008,
  kDefaultSampleSizePresent = 0x0010,
  kkDefaultSampleFlagsPresent = 0x0020,

  // This is `iso5` brand by spec on 14496-12, so can't be used
  // with brand of `isom`, `avc1`, `iso2`.

  // https://www.w3.org/TR/mse-byte-stream-format-isobmff/ said that
  // it should have `kDefaultBaseIsMoof', but not `kBaseDataOffsetPresent`.
  kDefaultBaseIsMoof = 0x020000,
};

enum class TrackFragmentRunFlags : uint16_t {
  kDataOffsetPresent = 0x0001,
  kFirstSampleFlagsPresent = 0x0004,
  kSampleDurationPresent = 0x0100,
  kSampleSizePresent = 0x0200,
  kSampleFlagsPresent = 0x0400,
};

enum class FragmentSampleFlags : uint32_t {
  kSampleFlagIsNonSync = 0x00010000,
  kSampleFlagDependsYes = 0x01000000,
  kSampleFlagDependsNo = 0x02000000,
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

// Bit Rate Box (`btrt`) box.
struct MEDIA_EXPORT BitRate : Box {
  uint32_t max_bit_rate;
  uint32_t avg_bit_rate;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Elementary Stream Descriptor (`esds`) box.
struct MEDIA_EXPORT ElementaryStreamDescriptor : FullBox {
  ElementaryStreamDescriptor();
  ~ElementaryStreamDescriptor();
  ElementaryStreamDescriptor(const ElementaryStreamDescriptor&);
  ElementaryStreamDescriptor& operator=(const ElementaryStreamDescriptor&);

  // ES descriptor 14496-1
  // DecoderConfigDescriptor (14496-1).
  // AAC AudioSpecificConfig (14496-3).
  std::vector<uint8_t> aac_codec_description;
};

// MP4A Audio Sample Entry (`mp4a`) box.
struct MEDIA_EXPORT AudioSampleEntry : Box {
  uint32_t sample_rate;  // AudioSampleEntry.

  ElementaryStreamDescriptor elementary_stream_descriptor;
  BitRate bit_rate;
};

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
  absl::optional<AudioSampleEntry> audio_sample_entry;
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

// Track Fragment Run (`trun`) box.
struct MEDIA_EXPORT TrackFragmentRun : FullBox {
  TrackFragmentRun();
  ~TrackFragmentRun();
  TrackFragmentRun(const TrackFragmentRun&);
  TrackFragmentRun& operator=(const TrackFragmentRun&);

  uint32_t sample_count;
  uint32_t first_sample_flags;

  // Optional fields, presence is indicated in `flags`. If not present, the
  // default value established in the `TrackFragmentHeader` is used.
  std::vector<base::TimeTicks> sample_timestamps;
  std::vector<uint32_t> sample_sizes;
  std::vector<uint32_t> sample_flags;
  // We don't support sample_composition_time_offsets as we don't know
  // how to get it on given data.
};

// Track Fragment Decode Time (`tfdt`) box.
struct MEDIA_EXPORT TrackFragmentDecodeTime : Box {
  base::TimeDelta base_media_decode_time;
};

// Track Fragment Header(`tfhd`) box.
struct MEDIA_EXPORT TrackFragmentHeader : FullBox {
  uint32_t track_id;

  // `base_data_offset` will be calculated during fragment write.
  // uint64_t base_data_offset;
  base::TimeDelta default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;
};

// Track Fragment Header(`traf`) box.
struct MEDIA_EXPORT TrackFragment : Box {
  TrackFragmentHeader header;
  TrackFragmentDecodeTime decode_time;
  TrackFragmentRun run;
};

// Movie Fragment Header(`mfhd`) box.
struct MEDIA_EXPORT MovieFragmentHeader : FullBox {
  uint32_t sequence_number;
};

// Movie Fragment (`moof`) box.
struct MEDIA_EXPORT MovieFragment : Box {
  MovieFragment();
  ~MovieFragment();
  MovieFragment(const MovieFragment&);
  MovieFragment& operator=(const MovieFragment&);

  MovieFragmentHeader header;
  std::vector<TrackFragment> track_fragments;
};

// Media Data (`mdat`) box.
struct MEDIA_EXPORT MediaData : Box {
  MediaData();
  ~MediaData();
  MediaData(const MediaData&);
  MediaData& operator=(const MediaData&);
  std::vector<std::vector<uint8_t>> track_data;
};

// File Type (`ftyp`) box.
struct MEDIA_EXPORT FileType : Box {
  FileType();
  ~FileType();
  uint32_t major_brand;
  uint32_t minor_version;
  std::vector<uint32_t> compatible_brands;
};

// Movie Track Fragment Random Access Box Entry.
struct TrackFragmentRandomAccessEntry {
  base::TimeDelta time;
  uint64_t moof_offset;
  uint32_t traf_number;
  uint32_t trun_number;
  uint32_t sample_number;
};

// Movie Track Fragment Random Access Box (`tfra`) box.
struct MEDIA_EXPORT TrackFragmentRandomAccess : FullBox {
  TrackFragmentRandomAccess();
  ~TrackFragmentRandomAccess();
  TrackFragmentRandomAccess(const TrackFragmentRandomAccess&);
  TrackFragmentRandomAccess& operator=(const TrackFragmentRandomAccess&);

  uint32_t track_id;
  std::vector<TrackFragmentRandomAccessEntry> entries;
};

// Movie Fragment Random Access Offset Box (`mfro`) box.
struct MEDIA_EXPORT FragmentRandomAccessOffset : Box {
  // It is `mfra` box size and should be located at the last of
  // enclosing `mfra` box.
};

// Movie Fragment Random Access Box (`mfra`) box.
struct MEDIA_EXPORT FragmentRandomAccess : Box {
  FragmentRandomAccess();
  ~FragmentRandomAccess();

  std::vector<TrackFragmentRandomAccess> tracks;
  FragmentRandomAccessOffset offset;
};

}  // namespace media::mp4::writable_boxes

#endif  // MEDIA_FORMATS_MP4_WRITABLE_BOX_DEFINITIONS_H_
