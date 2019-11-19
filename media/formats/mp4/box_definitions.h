// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_
#define MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/formats/mp4/aac.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_reader.h"
#include "media/formats/mp4/fourccs.h"
#include "media/media_buildflags.h"

namespace media {
namespace mp4 {

// Size in bytes needed to store largest IV.
const int kInitializationVectorSize = 16;

enum TrackType { kInvalid = 0, kVideo, kAudio, kText, kHint };

enum SampleFlags {
  kSampleIsNonSyncSample = 0x10000
};

#define DECLARE_BOX_METHODS(T)            \
  T();                                    \
  T(const T& other);                      \
  ~T() override;                          \
  bool Parse(BoxReader* reader) override; \
  FourCC BoxType() const override

struct MEDIA_EXPORT FileType : Box {
  DECLARE_BOX_METHODS(FileType);

  FourCC major_brand;
  uint32_t minor_version;
};

// If only copying the 'pssh' boxes, use ProtectionSystemSpecificHeader.
// If access to the individual fields is needed, use
// FullProtectionSystemSpecificHeader.
struct MEDIA_EXPORT ProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(ProtectionSystemSpecificHeader);

  std::vector<uint8_t> raw_box;
};

struct MEDIA_EXPORT FullProtectionSystemSpecificHeader : Box {
  DECLARE_BOX_METHODS(FullProtectionSystemSpecificHeader);

  std::vector<uint8_t> system_id;
  std::vector<std::vector<uint8_t>> key_ids;
  std::vector<uint8_t> data;
};

struct MEDIA_EXPORT SampleAuxiliaryInformationOffset : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationOffset);

  std::vector<uint64_t> offsets;
};

struct MEDIA_EXPORT SampleAuxiliaryInformationSize : Box {
  DECLARE_BOX_METHODS(SampleAuxiliaryInformationSize);

  uint8_t default_sample_info_size;
  uint32_t sample_count;
  std::vector<uint8_t> sample_info_sizes;
};

// Represent an entry in SampleEncryption box or CENC auxiliary info.
struct MEDIA_EXPORT SampleEncryptionEntry {
  SampleEncryptionEntry();
  SampleEncryptionEntry(const SampleEncryptionEntry& other);
  ~SampleEncryptionEntry();

  // Parse SampleEncryptionEntry from |reader|.
  // |iv_size| specifies the size of initialization vector. |has_subsamples|
  // indicates whether this sample encryption entry constains subsamples.
  // Returns false if parsing fails.
  bool Parse(BufferReader* reader, uint8_t iv_size, bool has_subsamples);

  // Get accumulated size of subsamples. Returns false if there is an overflow
  // anywhere.
  bool GetTotalSizeOfSubsamples(size_t* total_size) const;

  uint8_t initialization_vector[kInitializationVectorSize];
  std::vector<SubsampleEntry> subsamples;
};

// ISO/IEC 23001-7:2015 8.1.1.
struct MEDIA_EXPORT SampleEncryption : Box {
  enum SampleEncryptionFlags {
    kUseSubsampleEncryption = 2,
  };

  DECLARE_BOX_METHODS(SampleEncryption);

  bool use_subsample_encryption;
  // We may not know |iv_size| before reading this box, so we store the box
  // data for parsing later when |iv_size| is known.
  std::vector<uint8_t> sample_encryption_data;
};

struct MEDIA_EXPORT OriginalFormat : Box {
  DECLARE_BOX_METHODS(OriginalFormat);

  FourCC format;
};

struct MEDIA_EXPORT SchemeType : Box {
  DECLARE_BOX_METHODS(SchemeType);

  FourCC type;
  uint32_t version;
};

struct MEDIA_EXPORT TrackEncryption : Box {
  DECLARE_BOX_METHODS(TrackEncryption);

  // Note: this definition is specific to the CENC protection type.
  bool is_encrypted;
  uint8_t default_iv_size;
  std::vector<uint8_t> default_kid;
  uint8_t default_crypt_byte_block;
  uint8_t default_skip_byte_block;
  uint8_t default_constant_iv_size;
  uint8_t default_constant_iv[kInitializationVectorSize];
};

struct MEDIA_EXPORT SchemeInfo : Box {
  DECLARE_BOX_METHODS(SchemeInfo);

  TrackEncryption track_encryption;
};

struct MEDIA_EXPORT ProtectionSchemeInfo : Box {
  DECLARE_BOX_METHODS(ProtectionSchemeInfo);

  OriginalFormat format;
  SchemeType type;
  SchemeInfo info;

  bool HasSupportedScheme() const;
  bool IsCbcsEncryptionScheme() const;
};

struct MEDIA_EXPORT MovieHeader : Box {
  DECLARE_BOX_METHODS(MovieHeader);

  uint8_t version;
  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t timescale;
  uint64_t duration;
  int32_t rate;
  int16_t volume;
  // A 3x3 matrix of [ A B C ]
  //                 [ D E F ]
  //                 [ U V W ]
  // Where A-F are 16.16 fixed point decimals
  // And U, V, W are 2.30 fixed point decimals.
  DisplayMatrix display_matrix;
  uint32_t next_track_id;
};

struct MEDIA_EXPORT TrackHeader : Box {
  DECLARE_BOX_METHODS(TrackHeader);

  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t track_id;
  uint64_t duration;
  int16_t layer;
  int16_t alternate_group;
  int16_t volume;
  DisplayMatrix display_matrix;  // See MovieHeader.display_matrix
  uint32_t width;
  uint32_t height;
};

struct MEDIA_EXPORT EditListEntry {
  uint64_t segment_duration;
  int64_t media_time;
  int16_t media_rate_integer;
  int16_t media_rate_fraction;
};

struct MEDIA_EXPORT EditList : Box {
  DECLARE_BOX_METHODS(EditList);

  std::vector<EditListEntry> edits;
};

struct MEDIA_EXPORT Edit : Box {
  DECLARE_BOX_METHODS(Edit);

  EditList list;
};

struct MEDIA_EXPORT HandlerReference : Box {
  DECLARE_BOX_METHODS(HandlerReference);

  TrackType type;
  std::string name;
};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
struct MEDIA_EXPORT AVCDecoderConfigurationRecord : Box {
  DECLARE_BOX_METHODS(AVCDecoderConfigurationRecord);

  // Parses AVCDecoderConfigurationRecord data encoded in |data|.
  // Note: This method is intended to parse data outside the MP4StreamParser
  //       context and therefore the box header is not expected to be present
  //       in |data|.
  // Returns true if |data| was successfully parsed.
  bool Parse(const uint8_t* data, int data_size);

  uint8_t version;
  uint8_t profile_indication;
  uint8_t profile_compatibility;
  uint8_t avc_level;
  uint8_t length_size;

  typedef std::vector<uint8_t> SPS;
  typedef std::vector<uint8_t> PPS;

  std::vector<SPS> sps_list;
  std::vector<PPS> pps_list;

 private:
  bool ParseInternal(BufferReader* reader, MediaLog* media_log);
};
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

struct MEDIA_EXPORT VPCodecConfigurationRecord : Box {
  DECLARE_BOX_METHODS(VPCodecConfigurationRecord);

  VideoCodecProfile profile;
};

#if BUILDFLAG(ENABLE_AV1_DECODER)
struct MEDIA_EXPORT AV1CodecConfigurationRecord : Box {
  DECLARE_BOX_METHODS(AV1CodecConfigurationRecord);

  VideoCodecProfile profile;
};
#endif

struct MEDIA_EXPORT PixelAspectRatioBox : Box {
  DECLARE_BOX_METHODS(PixelAspectRatioBox);

  uint32_t h_spacing;
  uint32_t v_spacing;
};

struct MEDIA_EXPORT VideoSampleEntry : Box {
  DECLARE_BOX_METHODS(VideoSampleEntry);

  FourCC format;
  uint16_t data_reference_index;
  uint16_t width;
  uint16_t height;

  PixelAspectRatioBox pixel_aspect;
  ProtectionSchemeInfo sinf;

  VideoCodec video_codec;
  VideoCodecProfile video_codec_profile;

  bool IsFormatValid() const;

  scoped_refptr<BitstreamConverter> frame_bitstream_converter;
};

struct MEDIA_EXPORT ElementaryStreamDescriptor : Box {
  DECLARE_BOX_METHODS(ElementaryStreamDescriptor);

  uint8_t object_type;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  AAC aac;
#endif
};

struct MEDIA_EXPORT FlacSpecificBox : Box {
  DECLARE_BOX_METHODS(FlacSpecificBox);

  // We only care about the first metadata block, and it must be
  // METADATA_BLOCK_STREAM_INFO.

  // For FLAC decoder configuration, this is needed as extradata().
  // TODO(wolenetz,xhwang): MediaCodec or CDM decode of FLAC may need the
  // METADATA_BLOCK_HEADER, too (and if so, we may need to force the
  // last-metadata-block-flag in that header to be true, to allow us to ignore
  // non-streaminfo blocks.) Alternatively, the header can be reconstructed.
  // See https://crbug.com/747050.
  std::vector<uint8_t> stream_info;

  // MP4StreamParser needs this subset of |stream_info| parsed:
  uint32_t sample_rate;
  uint8_t channel_count;
  uint8_t bits_per_sample;
};

struct MEDIA_EXPORT OpusSpecificBox : Box {
  DECLARE_BOX_METHODS(OpusSpecificBox);
  std::vector<uint8_t> extradata;

  base::TimeDelta seek_preroll;
  uint16_t codec_delay_in_frames;
  uint8_t channel_count;
  uint32_t sample_rate;
};

struct MEDIA_EXPORT AudioSampleEntry : Box {
  DECLARE_BOX_METHODS(AudioSampleEntry);

  FourCC format;
  uint16_t data_reference_index;
  uint16_t channelcount;
  uint16_t samplesize;
  uint32_t samplerate;

  ProtectionSchemeInfo sinf;
  ElementaryStreamDescriptor esds;
  FlacSpecificBox dfla;
  OpusSpecificBox dops;
};

struct MEDIA_EXPORT SampleDescription : Box {
  DECLARE_BOX_METHODS(SampleDescription);

  TrackType type;
  std::vector<VideoSampleEntry> video_entries;
  std::vector<AudioSampleEntry> audio_entries;
};

struct MEDIA_EXPORT CencSampleEncryptionInfoEntry {
  CencSampleEncryptionInfoEntry();
  CencSampleEncryptionInfoEntry(const CencSampleEncryptionInfoEntry& other);
  ~CencSampleEncryptionInfoEntry();
  bool Parse(BoxReader* reader);

  bool is_encrypted;
  uint8_t iv_size;
  std::vector<uint8_t> key_id;
  uint8_t crypt_byte_block;
  uint8_t skip_byte_block;
  uint8_t constant_iv_size;
  uint8_t constant_iv[kInitializationVectorSize];
};

struct MEDIA_EXPORT SampleGroupDescription : Box {  // 'sgpd'.
  DECLARE_BOX_METHODS(SampleGroupDescription);

  uint32_t grouping_type;
  std::vector<CencSampleEncryptionInfoEntry> entries;
};

struct MEDIA_EXPORT SampleTable : Box {
  DECLARE_BOX_METHODS(SampleTable);

  // Media Source specific: we ignore many of the sub-boxes in this box,
  // including some that are required to be present in the BMFF spec. This
  // includes the 'stts', 'stsc', and 'stco' boxes, which must contain no
  // samples in order to be compliant files.
  SampleDescription description;
  SampleGroupDescription sample_group_description;
};

struct MEDIA_EXPORT MediaHeader : Box {
  DECLARE_BOX_METHODS(MediaHeader);

  std::string language() const;

  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t timescale;
  uint64_t duration;
  uint16_t language_code;
};

struct MEDIA_EXPORT MediaInformation : Box {
  DECLARE_BOX_METHODS(MediaInformation);

  SampleTable sample_table;
};

struct MEDIA_EXPORT Media : Box {
  DECLARE_BOX_METHODS(Media);

  MediaHeader header;
  HandlerReference handler;
  MediaInformation information;
};

struct MEDIA_EXPORT Track : Box {
  DECLARE_BOX_METHODS(Track);

  TrackHeader header;
  Media media;
  Edit edit;
};

struct MEDIA_EXPORT MovieExtendsHeader : Box {
  DECLARE_BOX_METHODS(MovieExtendsHeader);

  uint64_t fragment_duration;
};

struct MEDIA_EXPORT TrackExtends : Box {
  DECLARE_BOX_METHODS(TrackExtends);

  uint32_t track_id;
  uint32_t default_sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;
};

struct MEDIA_EXPORT MovieExtends : Box {
  DECLARE_BOX_METHODS(MovieExtends);

  MovieExtendsHeader header;
  std::vector<TrackExtends> tracks;
};

struct MEDIA_EXPORT Movie : Box {
  DECLARE_BOX_METHODS(Movie);

  bool fragmented;
  MovieHeader header;
  MovieExtends extends;
  std::vector<Track> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct MEDIA_EXPORT TrackFragmentDecodeTime : Box {
  DECLARE_BOX_METHODS(TrackFragmentDecodeTime);

  uint64_t decode_time;
};

struct MEDIA_EXPORT MovieFragmentHeader : Box {
  DECLARE_BOX_METHODS(MovieFragmentHeader);

  uint32_t sequence_number;
};

struct MEDIA_EXPORT TrackFragmentHeader : Box {
  DECLARE_BOX_METHODS(TrackFragmentHeader);

  uint32_t track_id;

  uint32_t sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

  // As 'flags' might be all zero, we cannot use zeroness alone to identify
  // when default_sample_flags wasn't specified, unlike the other values.
  bool has_default_sample_flags;
};

struct MEDIA_EXPORT TrackFragmentRun : Box {
  DECLARE_BOX_METHODS(TrackFragmentRun);

  uint32_t sample_count;
  uint32_t data_offset;
  std::vector<uint32_t> sample_flags;
  std::vector<uint32_t> sample_sizes;
  std::vector<uint32_t> sample_durations;
  std::vector<int32_t> sample_composition_time_offsets;
};

// sample_depends_on values in ISO/IEC 14496-12 Section 8.40.2.3.
enum SampleDependsOn {
  kSampleDependsOnUnknown = 0,
  kSampleDependsOnOthers = 1,
  kSampleDependsOnNoOther = 2,
  kSampleDependsOnReserved = 3,
};

class MEDIA_EXPORT IndependentAndDisposableSamples : public Box {
 public:
  DECLARE_BOX_METHODS(IndependentAndDisposableSamples);

  // Returns the SampleDependsOn value for the |i|'th value
  // in the track. If no data was parsed for the |i|'th sample,
  // then |kSampleDependsOnUnknown| is returned.
  SampleDependsOn sample_depends_on(size_t i) const;

 private:
  std::vector<SampleDependsOn> sample_depends_on_;
};

struct MEDIA_EXPORT SampleToGroupEntry {
  enum GroupDescriptionIndexBase {
    kTrackGroupDescriptionIndexBase = 0,
    kFragmentGroupDescriptionIndexBase = 0x10000,
  };

  uint32_t sample_count;
  uint32_t group_description_index;
};

struct MEDIA_EXPORT SampleToGroup : Box {  // 'sbgp'.
  DECLARE_BOX_METHODS(SampleToGroup);

  uint32_t grouping_type;
  uint32_t grouping_type_parameter;  // Version 1 only.
  std::vector<SampleToGroupEntry> entries;
};

struct MEDIA_EXPORT TrackFragment : Box {
  DECLARE_BOX_METHODS(TrackFragment);

  TrackFragmentHeader header;
  std::vector<TrackFragmentRun> runs;
  TrackFragmentDecodeTime decode_time;
  SampleAuxiliaryInformationOffset auxiliary_offset;
  SampleAuxiliaryInformationSize auxiliary_size;
  IndependentAndDisposableSamples sdtp;
  SampleGroupDescription sample_group_description;
  SampleToGroup sample_to_group;
  SampleEncryption sample_encryption;
};

struct MEDIA_EXPORT MovieFragment : Box {
  DECLARE_BOX_METHODS(MovieFragment);

  MovieFragmentHeader header;
  std::vector<TrackFragment> tracks;
  std::vector<ProtectionSystemSpecificHeader> pssh;
};

struct MEDIA_EXPORT ID3v2Box : Box {
  DECLARE_BOX_METHODS(ID3v2Box);

  // Up to a maximum of the first 128 bytes of the ID3v2 box.
  std::vector<uint8_t> id3v2_data;
};

struct MEDIA_EXPORT MetadataBox : Box {
  DECLARE_BOX_METHODS(MetadataBox);
  bool used_shaka_packager;
};

#undef DECLARE_BOX

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_BOX_DEFINITIONS_H_
