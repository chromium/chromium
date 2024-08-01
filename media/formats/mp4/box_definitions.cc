// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/box_definitions.h"

#include <bitset>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/formats/common/opus_constants.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/formats/mp4/rcheck.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include <optional>

#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/dolby_vision.h"
#include "media/parsers/h264_parser.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/formats/mp4/hevc.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace media {
namespace mp4 {

namespace {

const size_t kKeyIdSize = 16;
const size_t kFlacMetadataBlockStreaminfoSize = 34;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// Try to parse dvcC or dvvC box if exists, return `video_info` and an optional
// `dv_info` based on the configuration.
std::tuple<CodecProfileLevel, std::optional<CodecProfileLevel>> MaybeParseDOVI(
    BoxReader* reader,
    CodecProfileLevel video_info) {
  std::optional<DOVIDecoderConfigurationRecord> dovi_config;

  {
    DolbyVisionConfiguration dvcc;
    if (reader->HasChild(&dvcc) && reader->ReadChild(&dvcc)) {
      DVLOG(2) << __func__ << " reading DolbyVisionConfiguration (dvcC)";
      DCHECK_LE(dvcc.dovi_config.dv_profile, 7);
      dovi_config = dvcc.dovi_config;
    }
  }

  {
    DolbyVisionConfiguration8 dvvc;
    if (reader->HasChild(&dvvc) && reader->ReadChild(&dvvc)) {
      DVLOG(2) << __func__ << " reading DolbyVisionConfiguration (dvvC)";
      DCHECK_GT(dvvc.dovi_config.dv_profile, 7);
      dovi_config = dvvc.dovi_config;
    }
  }

  if (!dovi_config.has_value()) {
    return {video_info, std::nullopt};
  }

  constexpr int kHDR10CompatibilityId = 1;
  constexpr int kSDRCompatibilityId = 2;
  constexpr int kHLGCompatibilityId = 4;
  CodecProfileLevel dv_info = {VideoCodec::kDolbyVision,
                               dovi_config->codec_profile,
                               dovi_config->dv_level};
  if (dovi_config->dv_bl_signal_compatibility_id == kHDR10CompatibilityId ||
      dovi_config->dv_bl_signal_compatibility_id == kSDRCompatibilityId ||
      dovi_config->dv_bl_signal_compatibility_id == kHLGCompatibilityId) {
    return {video_info, dv_info};
  }
  // If the buffer is not backward compatible, always treat it as Dolby Vision.
  return {dv_info, std::nullopt};
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// Read color coordinate value as defined in the MasteringDisplayColorVolume
// ('mdcv') box.  Each coordinate is a float encoded in uint16_t, with upper
// bound set to 50000.
bool ReadMdcvColorCoordinate(BoxReader* reader,
                             float* normalized_value_in_float) {
  const float kColorCoordinateUpperBound = 50000.;

  uint16_t value_in_uint16;
  RCHECK(reader->Read2(&value_in_uint16));

  float value_in_float = static_cast<float>(value_in_uint16);

  if (value_in_float >= kColorCoordinateUpperBound) {
    *normalized_value_in_float = 1.f;
    return true;
  }

  *normalized_value_in_float = value_in_float / kColorCoordinateUpperBound;
  return true;
}

// Read "fixed point" value as defined in the
// SMPTE2086MasteringDisplayMetadataBox ('SmDm') box. See
// https://www.webmproject.org/vp9/mp4/#data-types-and-fields
bool ReadFixedPoint16(float fixed_point_divisor,
                      BoxReader* reader,
                      float* normalized_value_in_float) {
  uint16_t value;
  RCHECK(reader->Read2(&value));
  *normalized_value_in_float = value / fixed_point_divisor;
  return true;
}

bool ReadFixedPoint32(float fixed_point_divisor,
                      BoxReader* reader,
                      float* normalized_value_in_float) {
  uint32_t value;
  RCHECK(reader->Read4(&value));
  *normalized_value_in_float = value / fixed_point_divisor;
  return true;
}

gfx::HdrMetadataSmpteSt2086 ConvertMdcvToColorVolumeMetadata(
    const MasteringDisplayColorVolume& mdcv) {
  gfx::HdrMetadataSmpteSt2086 smpte_st_2086;
  smpte_st_2086.primaries = {
      mdcv.display_primaries_rx, mdcv.display_primaries_ry,
      mdcv.display_primaries_gx, mdcv.display_primaries_gy,
      mdcv.display_primaries_bx, mdcv.display_primaries_by,
      mdcv.white_point_x,        mdcv.white_point_y,
  };
  smpte_st_2086.luminance_max = mdcv.max_display_mastering_luminance;
  smpte_st_2086.luminance_min = mdcv.min_display_mastering_luminance;

  return smpte_st_2086;
}

}  // namespace

FileType::FileType() = default;
FileType::FileType(const FileType& other) = default;
FileType::~FileType() = default;
FourCC FileType::BoxType() const { return FOURCC_FTYP; }

bool FileType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFourCC(&major_brand) && reader->Read4(&minor_version));
  size_t num_brands = (reader->box_size() - reader->pos()) / sizeof(FourCC);
  return reader->SkipBytes(sizeof(FourCC) * num_brands);  // compatible_brands
}

ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader() = default;
ProtectionSystemSpecificHeader::ProtectionSystemSpecificHeader(
    const ProtectionSystemSpecificHeader& other) = default;
ProtectionSystemSpecificHeader::~ProtectionSystemSpecificHeader() = default;
FourCC ProtectionSystemSpecificHeader::BoxType() const { return FOURCC_PSSH; }

bool ProtectionSystemSpecificHeader::Parse(BoxReader* reader) {
  // Don't bother validating the box's contents.
  // Copy the entire box, including the header, for passing to EME as initData.
  DCHECK(raw_box.empty());
  base::span<const uint8_t> buffer = reader->buffer().first(reader->box_size());
  raw_box.assign(buffer.begin(), buffer.end());
  return true;
}

FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader() =
    default;
FullProtectionSystemSpecificHeader::FullProtectionSystemSpecificHeader(
    const FullProtectionSystemSpecificHeader& other) = default;
FullProtectionSystemSpecificHeader::~FullProtectionSystemSpecificHeader() =
    default;
FourCC FullProtectionSystemSpecificHeader::BoxType() const {
  return FOURCC_PSSH;
}

// The format of a 'pssh' box is as follows:
//   unsigned int(32) size;
//   unsigned int(32) type = "pssh";
//   if (size==1) {
//     unsigned int(64) largesize;
//   } else if (size==0) {
//     -- box extends to end of file
//   }
//   unsigned int(8) version;
//   bit(24) flags;
//   unsigned int(8)[16] SystemID;
//   if (version > 0)
//   {
//     unsigned int(32) KID_count;
//     {
//       unsigned int(8)[16] KID;
//     } [KID_count]
//   }
//   unsigned int(32) DataSize;
//   unsigned int(8)[DataSize] Data;

bool FullProtectionSystemSpecificHeader::Parse(mp4::BoxReader* reader) {
  RCHECK(reader->type() == BoxType() && reader->ReadFullBoxHeader());

  // Only versions 0 and 1 of the 'pssh' boxes are supported. Any other
  // versions are ignored.
  RCHECK(reader->version() == 0 || reader->version() == 1);
  RCHECK(reader->flags() == 0);
  RCHECK(reader->ReadVec(&system_id, 16));

  if (reader->version() > 0) {
    uint32_t kid_count;
    RCHECK(reader->Read4(&kid_count));
    for (uint32_t i = 0; i < kid_count; ++i) {
      std::vector<uint8_t> kid;
      RCHECK(reader->ReadVec(&kid, 16));
      key_ids.push_back(kid);
    }
  }

  uint32_t data_size;
  RCHECK(reader->Read4(&data_size));
  RCHECK(reader->ReadVec(&data, data_size));
  return true;
}

SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset() = default;
SampleAuxiliaryInformationOffset::SampleAuxiliaryInformationOffset(
    const SampleAuxiliaryInformationOffset& other) = default;
SampleAuxiliaryInformationOffset::~SampleAuxiliaryInformationOffset() = default;
FourCC SampleAuxiliaryInformationOffset::BoxType() const { return FOURCC_SAIO; }

bool SampleAuxiliaryInformationOffset::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  uint32_t count;
  RCHECK(reader->Read4(&count));
  int bytes_per_offset = reader->version() == 1 ? 8 : 4;

  // Cast |count| to size_t before multiplying to support maximum platform size.
  base::CheckedNumeric<size_t> bytes_needed =
      base::CheckMul(bytes_per_offset, static_cast<size_t>(count));
  RCHECK_MEDIA_LOGGED(bytes_needed.IsValid(), reader->media_log(),
                      "Extreme SAIO count exceeds implementation limit.");
  RCHECK(reader->HasBytes(bytes_needed.ValueOrDie()));

  RCHECK(count <= offsets.max_size());
  offsets.resize(count);

  for (uint32_t i = 0; i < count; i++) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&offsets[i]));
    } else {
      RCHECK(reader->Read4Into8(&offsets[i]));
    }
  }
  return true;
}

SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize()
  : default_sample_info_size(0), sample_count(0) {
}
SampleAuxiliaryInformationSize::SampleAuxiliaryInformationSize(
    const SampleAuxiliaryInformationSize& other) = default;
SampleAuxiliaryInformationSize::~SampleAuxiliaryInformationSize() = default;
FourCC SampleAuxiliaryInformationSize::BoxType() const { return FOURCC_SAIZ; }

bool SampleAuxiliaryInformationSize::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->flags() & 1)
    RCHECK(reader->SkipBytes(8));

  RCHECK(reader->Read1(&default_sample_info_size) &&
         reader->Read4(&sample_count));
  if (default_sample_info_size == 0)
    return reader->ReadVec(&sample_info_sizes, sample_count);
  return true;
}

SampleEncryptionEntry::SampleEncryptionEntry() = default;
SampleEncryptionEntry::SampleEncryptionEntry(
    const SampleEncryptionEntry& other) = default;
SampleEncryptionEntry::~SampleEncryptionEntry() = default;

bool SampleEncryptionEntry::Parse(BufferReader* reader,
                                  uint8_t iv_size,
                                  bool has_subsamples) {
  // According to ISO/IEC FDIS 23001-7: CENC spec, IV should be either
  // 64-bit (8-byte) or 128-bit (16-byte). The 3rd Edition allows |iv_size|
  // to be 0, for the case of a "constant IV". In this case, the existence of
  // the constant IV must be ensured by the caller.
  RCHECK(iv_size == 0 || iv_size == 8 || iv_size == 16);

  memset(initialization_vector, 0, sizeof(initialization_vector));
  for (uint8_t i = 0; i < iv_size; i++)
    RCHECK(reader->Read1(initialization_vector + i));

  if (!has_subsamples) {
    subsamples.clear();
    return true;
  }

  uint16_t subsample_count;
  RCHECK(reader->Read2(&subsample_count));
  RCHECK(subsample_count > 0);
  RCHECK(subsample_count <= subsamples.max_size());
  subsamples.resize(subsample_count);
  for (SubsampleEntry& subsample : subsamples) {
    uint16_t clear_bytes;
    uint32_t cypher_bytes;
    RCHECK(reader->Read2(&clear_bytes) && reader->Read4(&cypher_bytes));
    subsample.clear_bytes = clear_bytes;
    subsample.cypher_bytes = cypher_bytes;
  }
  return true;
}

bool SampleEncryptionEntry::GetTotalSizeOfSubsamples(size_t* total_size) const {
  size_t size = 0;
  for (const SubsampleEntry& subsample : subsamples) {
    size += subsample.clear_bytes;
    RCHECK(size >= subsample.clear_bytes);  // overflow
    size += subsample.cypher_bytes;
    RCHECK(size >= subsample.cypher_bytes);  // overflow
  }
  *total_size = size;
  return true;
}

SampleEncryption::SampleEncryption() : use_subsample_encryption(false) {}
SampleEncryption::SampleEncryption(const SampleEncryption& other) = default;
SampleEncryption::~SampleEncryption() = default;
FourCC SampleEncryption::BoxType() const {
  return FOURCC_SENC;
}

bool SampleEncryption::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  use_subsample_encryption = (reader->flags() & kUseSubsampleEncryption) != 0;
  base::span<const uint8_t> buffer =
      reader->buffer().first(reader->box_size()).subspan(reader->pos());
  sample_encryption_data.assign(buffer.begin(), buffer.end());
  return true;
}

OriginalFormat::OriginalFormat() : format(FOURCC_NULL) {}
OriginalFormat::OriginalFormat(const OriginalFormat& other) = default;
OriginalFormat::~OriginalFormat() = default;
FourCC OriginalFormat::BoxType() const { return FOURCC_FRMA; }

bool OriginalFormat::Parse(BoxReader* reader) {
  return reader->ReadFourCC(&format);
}

SchemeType::SchemeType() : type(FOURCC_NULL), version(0) {}
SchemeType::SchemeType(const SchemeType& other) = default;
SchemeType::~SchemeType() = default;
FourCC SchemeType::BoxType() const { return FOURCC_SCHM; }

bool SchemeType::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->ReadFourCC(&type) &&
         reader->Read4(&version));
  return true;
}

TrackEncryption::TrackEncryption()
    : is_encrypted(false),
      default_iv_size(0),
      default_crypt_byte_block(0),
      default_skip_byte_block(0),
      default_constant_iv_size(0) {}
TrackEncryption::TrackEncryption(const TrackEncryption& other) = default;
TrackEncryption::~TrackEncryption() = default;
FourCC TrackEncryption::BoxType() const { return FOURCC_TENC; }

bool TrackEncryption::Parse(BoxReader* reader) {
  uint8_t flag;
  uint8_t possible_pattern_info;
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->SkipBytes(1) &&  // skip reserved byte
         reader->Read1(&possible_pattern_info) && reader->Read1(&flag) &&
         reader->Read1(&default_iv_size) &&
         reader->ReadVec(&default_kid, kKeyIdSize));
  is_encrypted = (flag != 0);
  if (is_encrypted) {
    if (reader->version() > 0) {
      default_crypt_byte_block = (possible_pattern_info >> 4) & 0x0f;
      default_skip_byte_block = possible_pattern_info & 0x0f;
    }
    if (default_iv_size == 0) {
      RCHECK(reader->Read1(&default_constant_iv_size));
      RCHECK(default_constant_iv_size == 8 || default_constant_iv_size == 16);
      memset(default_constant_iv, 0, sizeof(default_constant_iv));
      for (uint8_t i = 0; i < default_constant_iv_size; i++)
        RCHECK(reader->Read1(default_constant_iv + i));
    } else {
      RCHECK(default_iv_size == 8 || default_iv_size == 16);
    }
  } else {
    RCHECK(default_iv_size == 0);
  }
  return true;
}

SchemeInfo::SchemeInfo() = default;
SchemeInfo::SchemeInfo(const SchemeInfo& other) = default;
SchemeInfo::~SchemeInfo() = default;
FourCC SchemeInfo::BoxType() const { return FOURCC_SCHI; }

bool SchemeInfo::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&track_encryption);
}

ProtectionSchemeInfo::ProtectionSchemeInfo() = default;
ProtectionSchemeInfo::ProtectionSchemeInfo(const ProtectionSchemeInfo& other) =
    default;
ProtectionSchemeInfo::~ProtectionSchemeInfo() = default;
FourCC ProtectionSchemeInfo::BoxType() const { return FOURCC_SINF; }

bool ProtectionSchemeInfo::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&format) &&
         reader->ReadChild(&type));
  if (HasSupportedScheme())
    RCHECK(reader->ReadChild(&info));
  // Other protection schemes are silently ignored. Since the protection scheme
  // type can't be determined until this box is opened, we return 'true' for
  // unsupported protection schemes. It is the parent box's responsibility to
  // ensure that this scheme type is a supported one.
  return true;
}

bool ProtectionSchemeInfo::HasSupportedScheme() const {
  FourCC four_cc = type.type;
  return (four_cc == FOURCC_CENC || four_cc == FOURCC_CBCS);
}

bool ProtectionSchemeInfo::IsCbcsEncryptionScheme() const {
  FourCC four_cc = type.type;
  return (four_cc == FOURCC_CBCS);
}

MovieHeader::MovieHeader()
    : version(0),
      creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      rate(-1),
      volume(-1),
      next_track_id(0) {}
MovieHeader::MovieHeader(const MovieHeader& other) = default;
MovieHeader::~MovieHeader() = default;
FourCC MovieHeader::BoxType() const { return FOURCC_MVHD; }

bool MovieHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  version = reader->version();

  if (version == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) &&
           reader->Read4Into8(&duration));
  }

  RCHECK_MEDIA_LOGGED(timescale > 0, reader->media_log(),
                      "Movie header's timescale must not be 0");

  RCHECK(reader->Read4s(&rate) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(10) &&  // reserved
         reader->ReadDisplayMatrix(display_matrix) &&
         reader->SkipBytes(24) &&  // predefined zero
         reader->Read4(&next_track_id));
  return true;
}

TrackHeader::TrackHeader()
    : creation_time(0),
      modification_time(0),
      track_id(0),
      duration(0),
      layer(-1),
      alternate_group(-1),
      volume(-1),
      width(0),
      height(0) {}
TrackHeader::TrackHeader(const TrackHeader& other) = default;
TrackHeader::~TrackHeader() = default;
FourCC TrackHeader::BoxType() const { return FOURCC_TKHD; }

bool TrackHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) &&
           reader->Read8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&    // reserved
           reader->Read8(&duration));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&track_id) &&
           reader->SkipBytes(4) &&   // reserved
           reader->Read4Into8(&duration));
  }

  RCHECK(reader->SkipBytes(8) &&  // reserved
         reader->Read2s(&layer) &&
         reader->Read2s(&alternate_group) &&
         reader->Read2s(&volume) &&
         reader->SkipBytes(2) &&  // reserved
         reader->ReadDisplayMatrix(display_matrix) &&
         reader->Read4(&width) &&
         reader->Read4(&height));

  // Round width and height to the nearest number.
  // Note: width and height are fixed-point 16.16 values. The following code
  // rounds a.1x to a + 1, and a.0x to a.
  width >>= 15;
  width += 1;
  width >>= 1;
  height >>= 15;
  height += 1;
  height >>= 1;

  return true;
}

SampleDescription::SampleDescription() : type(kInvalid) {}
SampleDescription::SampleDescription(const SampleDescription& other) = default;
SampleDescription::~SampleDescription() = default;
FourCC SampleDescription::BoxType() const { return FOURCC_STSD; }

bool SampleDescription::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->SkipBytes(4) &&
         reader->Read4(&count));
  video_entries.clear();
  audio_entries.clear();

  // Note: this value is preset before scanning begins. See comments in the
  // Parse(Media*) function.
  if (type == kVideo) {
    RCHECK(reader->ReadAllChildren(&video_entries));
  } else if (type == kAudio) {
    RCHECK(reader->ReadAllChildren(&audio_entries));
  }
  return true;
}

SampleTable::SampleTable() = default;
SampleTable::SampleTable(const SampleTable& other) = default;

SampleTable::~SampleTable() = default;
FourCC SampleTable::BoxType() const { return FOURCC_STBL; }

bool SampleTable::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&description));
  // There could be multiple SampleGroupDescription boxes with different
  // grouping types. For common encryption, the relevant grouping type is
  // 'seig'. Continue reading until 'seig' is found, or until running out of
  // child boxes.
  while (reader->HasChild(&sample_group_description)) {
    RCHECK(reader->ReadChild(&sample_group_description));
    if (sample_group_description.grouping_type == FOURCC_SEIG)
      break;
    sample_group_description.entries.clear();
  }
  return true;
}

EditList::EditList() = default;
EditList::EditList(const EditList& other) = default;
EditList::~EditList() = default;
FourCC EditList::BoxType() const { return FOURCC_ELST; }

bool EditList::Parse(BoxReader* reader) {
  uint32_t count;
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&count));

  const size_t bytes_per_edit = reader->version() == 1 ? 20 : 12;

  // Cast |count| to size_t before multiplying to support maximum platform size.
  base::CheckedNumeric<size_t> bytes_needed =
      base::CheckMul(bytes_per_edit, static_cast<size_t>(count));
  RCHECK_MEDIA_LOGGED(bytes_needed.IsValid(), reader->media_log(),
                      "Extreme ELST count exceeds implementation limit.");
  RCHECK(reader->HasBytes(bytes_needed.ValueOrDie()));

  RCHECK(count <= edits.max_size());
  edits.resize(count);

  for (auto edit = edits.begin(); edit != edits.end(); ++edit) {
    if (reader->version() == 1) {
      RCHECK(reader->Read8(&edit->segment_duration) &&
             reader->Read8s(&edit->media_time));
    } else {
      RCHECK(reader->Read4Into8(&edit->segment_duration) &&
             reader->Read4sInto8s(&edit->media_time));
    }
    RCHECK(reader->Read2s(&edit->media_rate_integer) &&
           reader->Read2s(&edit->media_rate_fraction));
  }
  return true;
}

Edit::Edit() = default;
Edit::Edit(const Edit& other) = default;
Edit::~Edit() = default;
FourCC Edit::BoxType() const { return FOURCC_EDTS; }

bool Edit::Parse(BoxReader* reader) {
  return reader->ScanChildren() && reader->ReadChild(&list);
}

HandlerReference::HandlerReference() : type(kInvalid) {}
HandlerReference::HandlerReference(const HandlerReference& other) = default;
HandlerReference::~HandlerReference() = default;
FourCC HandlerReference::BoxType() const { return FOURCC_HDLR; }

bool HandlerReference::Parse(BoxReader* reader) {
  FourCC hdlr_type;
  RCHECK(reader->ReadFullBoxHeader() && reader->SkipBytes(4) &&
         reader->ReadFourCC(&hdlr_type) && reader->SkipBytes(12));

  // Now we should be at the beginning of the |name| field of HDLR box. The
  // |name| is a zero-terminated ASCII string in ISO BMFF, but it was a
  // Pascal-style counted string in older QT/Mov formats. So we'll read the
  // remaining box bytes first, then if the last one is zero, we strip the last
  // zero byte, otherwise we'll string the first byte (containing the length of
  // the Pascal-style string).
  std::vector<uint8_t> name_bytes;
  RCHECK(reader->ReadVec(&name_bytes, reader->box_size() - reader->pos()));
  if (name_bytes.size() == 0) {
    name = "";
  } else if (name_bytes.back() == 0) {
    // This is a zero-terminated C-style string, exclude the last byte.
    name = std::string(name_bytes.begin(), name_bytes.end() - 1);
  } else {
    // Check that the length of the Pascal-style string is correct.
    RCHECK(name_bytes[0] == (name_bytes.size() - 1));
    // Skip the first byte, containing the length of the Pascal-string.
    name = std::string(name_bytes.begin() + 1, name_bytes.end());
  }

  if (hdlr_type == FOURCC_VIDE) {
    type = kVideo;
  } else if (hdlr_type == FOURCC_SOUN) {
    type = kAudio;
  } else if (hdlr_type == FOURCC_META || hdlr_type == FOURCC_SUBT ||
             hdlr_type == FOURCC_TEXT || hdlr_type == FOURCC_SBTL) {
    // For purposes of detection, we include 'sbtl' handler here. Note, though
    // that ISO-14496-12 and its 2012 Amendment 2, and the spec for sourcing
    // inband tracks all reference only 'text' or 'subt', and 14496-30
    // references only 'subt'. Yet ffmpeg can encode subtitles as 'sbtl'.
    type = kText;
  } else {
    type = kInvalid;
  }
  return true;
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord()
    : version(0),
      profile_indication(0),
      profile_compatibility(0),
      avc_level(0),
      length_size(0) {}
AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord(
    const AVCDecoderConfigurationRecord& other) = default;
AVCDecoderConfigurationRecord::~AVCDecoderConfigurationRecord() = default;
FourCC AVCDecoderConfigurationRecord::BoxType() const { return FOURCC_AVCC; }

bool AVCDecoderConfigurationRecord::Parse(BoxReader* reader) {
  return ParseInternal(reader, reader->media_log());
}

bool AVCDecoderConfigurationRecord::Parse(const uint8_t* data, int data_size) {
  BufferReader reader(data, data_size);
  NullMediaLog media_log;
  return ParseInternal(&reader, &media_log);
}

bool AVCDecoderConfigurationRecord::ParseInternal(BufferReader* reader,
                                                  MediaLog* media_log) {
  RCHECK(reader->Read1(&version) && version == 1 &&
         reader->Read1(&profile_indication) &&
         reader->Read1(&profile_compatibility) &&
         reader->Read1(&avc_level));

  uint8_t length_size_minus_one;
  RCHECK(reader->Read1(&length_size_minus_one));
  length_size = (length_size_minus_one & 0x3) + 1;

  RCHECK(length_size != 3); // Only values of 1, 2, and 4 are valid.

  uint8_t num_sps;
  RCHECK(reader->Read1(&num_sps));
  num_sps &= 0x1f;

  sps_list.resize(num_sps);
  for (int i = 0; i < num_sps; i++) {
    uint16_t sps_length;
    RCHECK(reader->Read2(&sps_length) &&
           reader->ReadVec(&sps_list[i], sps_length));
    RCHECK(sps_list[i].size() > 4);
  }

  uint8_t num_pps;
  RCHECK(reader->Read1(&num_pps));

  pps_list.resize(num_pps);
  for (int i = 0; i < num_pps; i++) {
    uint16_t pps_length;
    RCHECK(reader->Read2(&pps_length) &&
           reader->ReadVec(&pps_list[i], pps_length));
  }

  if (profile_indication == 100 || profile_indication == 110 ||
      profile_indication == 122 || profile_indication == 144) {
    if (!ParseREXT(reader, media_log)) {
      DVLOG(2) << __func__ << ": avcC REXT is missing or invalid";
      chroma_format = 0;
      bit_depth_luma_minus8 = 0;
      bit_depth_chroma_minus8 = 0;
      sps_ext_list.resize(0);
    }
  }

  return true;
}

bool AVCDecoderConfigurationRecord::ParseREXT(BufferReader* reader,
                                              MediaLog* media_log) {
  RCHECK(reader->Read1(&chroma_format));
  chroma_format &= 0x3;

  RCHECK(reader->Read1(&bit_depth_luma_minus8));
  bit_depth_luma_minus8 &= 0x7;
  RCHECK(bit_depth_luma_minus8 <= 4);

  RCHECK(reader->Read1(&bit_depth_chroma_minus8));
  bit_depth_chroma_minus8 &= 0x7;
  RCHECK(bit_depth_chroma_minus8 <= 4);

  uint8_t num_sps_ext;
  RCHECK(reader->Read1(&num_sps_ext));

  sps_ext_list.resize(num_sps_ext);
  for (int i = 0; i < num_sps_ext; i++) {
    uint16_t sps_ext_length;
    RCHECK(reader->Read2(&sps_ext_length) &&
           reader->ReadVec(&sps_ext_list[i], sps_ext_length));
  }

  return true;
}

bool AVCDecoderConfigurationRecord::Serialize(
    std::vector<uint8_t>& output) const {
  if (sps_list.size() > 0x1f) {
    return false;
  }

  if (pps_list.size() > 0xff) {
    return false;
  }

  if (length_size != 1 && length_size != 2 && length_size != 4) {
    return false;
  }

  size_t expected_size =
      1 +  // configurationVersion
      1 +  // AVCProfileIndication
      1 +  // profile_compatibility
      1 +  // AVCLevelIndication
      1 +  // lengthSizeMinusOne
      1 +  // numOfSequenceParameterSets, i.e. length of sps_list
      1;   // numOfPictureParameterSets, i.e. length of pps_list

  for (auto& sps : sps_list) {
    expected_size += 2;  // sequenceParameterSetLength
    if (sps.size() > 0xffff) {
      return false;
    }
    expected_size += sps.size();
  }

  for (auto& pps : pps_list) {
    expected_size += 2;  // pictureParameterSetLength
    if (pps.size() > 0xffff) {
      return false;
    }
    expected_size += pps.size();
  }

  if (profile_indication == 100 || profile_indication == 110 ||
      profile_indication == 122 || profile_indication == 144) {
    if (chroma_format > 0x3) {
      return false;
    }

    if (bit_depth_luma_minus8 > 4) {
      return false;
    }

    if (bit_depth_chroma_minus8 > 4) {
      return false;
    }

    if (sps_ext_list.size() > 0xff) {
      return false;
    }

    expected_size += 1 +  // chroma_format
                     1 +  // bit_depth_luma_minus8
                     1 +  // bit_depth_chroma_minus8
                     1;   // numOfSequenceParameterSetExt

    for (auto& sps_ext : sps_ext_list) {
      expected_size += 2;  // sequenceParameterSetExtLength
      if (sps_ext.size() > 0xffff) {
        return false;
      }
      expected_size += sps_ext.size();
    }
  }

  output.clear();
  output.resize(expected_size);
  auto writer = base::SpanWriter(base::span(output));
  bool result = true;

  // configurationVersion
  result &= writer.WriteU8BigEndian(version);
  // AVCProfileIndication
  result &= writer.WriteU8BigEndian(profile_indication);
  // profile_compatibility
  result &= writer.WriteU8BigEndian(profile_compatibility);
  // AVCLevelIndication
  result &= writer.WriteU8BigEndian(avc_level);
  // lengthSizeMinusOne
  uint8_t length_size_minus_one = (length_size - 1) | 0xfc;
  result &= writer.WriteU8BigEndian(length_size_minus_one);
  // numOfSequenceParameterSets
  uint8_t sps_size = sps_list.size() | 0xe0;
  result &= writer.WriteU8BigEndian(sps_size);
  // sequenceParameterSetNALUnits
  for (auto& sps : sps_list) {
    result &= writer.WriteU16BigEndian(sps.size());
    result &= writer.Write(sps);
  }
  // numOfPictureParameterSets
  uint8_t pps_size = pps_list.size();
  result &= writer.WriteU8BigEndian(pps_size);
  // pictureParameterSetNALUnit
  for (auto& pps : pps_list) {
    result &= writer.WriteU16BigEndian(pps.size());
    result &= writer.Write(pps);
  }

  if (profile_indication == 100 || profile_indication == 110 ||
      profile_indication == 122 || profile_indication == 144) {
    // chroma_format
    result &= writer.WriteU8BigEndian(chroma_format | 0xfc);
    // bit_depth_luma_minus8
    result &= writer.WriteU8BigEndian(bit_depth_luma_minus8 | 0xf8);
    // bit_depth_chroma_minus8
    result &= writer.WriteU8BigEndian(bit_depth_chroma_minus8 | 0xf8);
    // numOfSequenceParameterSetExt
    uint8_t sps_ext_size = sps_ext_list.size();
    result &= writer.WriteU8BigEndian(sps_ext_size);
    // sequenceParameterSetExtNALUnit
    for (auto& sps_ext : sps_ext_list) {
      result &= writer.WriteU16BigEndian(sps_ext.size());
      result &= writer.Write(sps_ext);
    }
  }

  return result;
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

VPCodecConfigurationRecord::VPCodecConfigurationRecord()
    : profile(VIDEO_CODEC_PROFILE_UNKNOWN) {}

VPCodecConfigurationRecord::VPCodecConfigurationRecord(
    const VPCodecConfigurationRecord& other) = default;

VPCodecConfigurationRecord::~VPCodecConfigurationRecord() = default;

FourCC VPCodecConfigurationRecord::BoxType() const {
  return FOURCC_VPCC;
}

bool VPCodecConfigurationRecord::Parse(BoxReader* reader) {
  uint8_t profile_indication = 0;
  RCHECK(reader->ReadFullBoxHeader() && reader->Read1(&profile_indication));
  // The remaining fields are not parsed as we don't care about them for now.

  switch (profile_indication) {
    case 0:
      profile = VP9PROFILE_PROFILE0;
      break;
    case 1:
      profile = VP9PROFILE_PROFILE1;
      break;
    case 2:
      profile = VP9PROFILE_PROFILE2;
      break;
    case 3:
      profile = VP9PROFILE_PROFILE3;
      break;
    default:
      MEDIA_LOG(ERROR, reader->media_log())
          << "Unsupported VP9 profile: 0x" << std::hex
          << static_cast<uint32_t>(profile_indication);
      return false;
  }

  RCHECK(reader->Read1(&level));

  uint8_t depth_chroma_full_range;
  RCHECK(reader->Read1(&depth_chroma_full_range));

  uint8_t primary_id;
  RCHECK(reader->Read1(&primary_id));

  uint8_t transfer_id;
  RCHECK(reader->Read1(&transfer_id));

  uint8_t matrix_id;
  RCHECK(reader->Read1(&matrix_id));

  color_space = VideoColorSpace(primary_id, transfer_id, matrix_id,
                                depth_chroma_full_range & 1
                                    ? gfx::ColorSpace::RangeID::FULL
                                    : gfx::ColorSpace::RangeID::LIMITED);
  return true;
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
AV1CodecConfigurationRecord::AV1CodecConfigurationRecord() = default;

AV1CodecConfigurationRecord::AV1CodecConfigurationRecord(
    const AV1CodecConfigurationRecord& other) = default;

AV1CodecConfigurationRecord::~AV1CodecConfigurationRecord() = default;

FourCC AV1CodecConfigurationRecord::BoxType() const {
  return FOURCC_AV1C;
}

bool AV1CodecConfigurationRecord::Parse(BoxReader* reader) {
  return ParseInternal(reader, reader->media_log());
}

bool AV1CodecConfigurationRecord::Parse(const uint8_t* data, int data_size) {
  BufferReader reader(data, data_size);
  NullMediaLog media_log;
  return ParseInternal(&reader, &media_log);
}

// Parse the AV1CodecConfigurationRecord, which has the following format:
// unsigned int (1) marker = 1;
// unsigned int (7) version = 1;
// unsigned int (3) seq_profile;
// unsigned int (5) seq_level_idx_0;
// unsigned int (1) seq_tier_0;
// unsigned int (1) high_bitdepth;
// unsigned int (1) twelve_bit;
// unsigned int (1) monochrome;
// unsigned int (1) chroma_subsampling_x;
// unsigned int (1) chroma_subsampling_y;
// unsigned int (2) chroma_sample_position;
// unsigned int (3) reserved = 0;
//
// unsigned int (1) initial_presentation_delay_present;
// if (initial_presentation_delay_present) {
//   unsigned int (4) initial_presentation_delay_minus_one;
// } else {
//   unsigned int (4) reserved = 0;
// }
//
// unsigned int (8)[] configOBUs;
bool AV1CodecConfigurationRecord::ParseInternal(BufferReader* reader,
                                                MediaLog* media_log) {
  uint8_t av1c_byte = 0;
  RCHECK(reader->Read1(&av1c_byte));
  const uint8_t av1c_marker = av1c_byte >> 7;
  if (!av1c_marker) {
    MEDIA_LOG(ERROR, media_log) << "Unsupported av1C: marker unset.";
    return false;
  }

  const uint8_t av1c_version = av1c_byte & 0b01111111;
  if (av1c_version != 1) {
    MEDIA_LOG(ERROR, media_log)
        << "Unsupported av1C: unexpected version number: " << av1c_version;
    return false;
  }

  RCHECK(reader->Read1(&av1c_byte));
  const uint8_t seq_profile = av1c_byte >> 5;
  switch (seq_profile) {
    case 0:
      profile = AV1PROFILE_PROFILE_MAIN;
      break;
    case 1:
      profile = AV1PROFILE_PROFILE_HIGH;
      break;
    case 2:
      profile = AV1PROFILE_PROFILE_PRO;
      break;
    default:
      MEDIA_LOG(ERROR, media_log)
          << "Unsupported av1C: unknown profile 0x" << std::hex << seq_profile;
      return false;
  }

  // The remaining fields are ignored since we don't care about them yet.

  return true;
}
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)

PixelAspectRatioBox::PixelAspectRatioBox() : h_spacing(1), v_spacing(1) {}
PixelAspectRatioBox::PixelAspectRatioBox(const PixelAspectRatioBox& other) =
    default;
PixelAspectRatioBox::~PixelAspectRatioBox() = default;
FourCC PixelAspectRatioBox::BoxType() const { return FOURCC_PASP; }

bool PixelAspectRatioBox::Parse(BoxReader* reader) {
  RCHECK(reader->Read4(&h_spacing) && reader->Read4(&v_spacing));
  return true;
}

ColorParameterInformation::ColorParameterInformation() = default;
ColorParameterInformation::ColorParameterInformation(
    const ColorParameterInformation& other) = default;
ColorParameterInformation::~ColorParameterInformation() = default;
FourCC ColorParameterInformation::BoxType() const {
  return FOURCC_COLR;
}

bool ColorParameterInformation::Parse(BoxReader* reader) {
  fully_parsed = false;

  FourCC type;
  RCHECK(reader->ReadFourCC(&type));

  if (type != FOURCC_NCLX) {
    // Ignore currently unsupported color information metadata parsing.
    // TODO: Support 'nclc', 'rICC', and 'prof'.
    return true;
  }

  uint8_t full_range_byte;
  RCHECK(reader->Read2(&colour_primaries) &&
         reader->Read2(&transfer_characteristics) &&
         reader->Read2(&matrix_coefficients) &&
         reader->Read1(&full_range_byte));
  full_range = full_range_byte & 0x80;
  fully_parsed = true;

  return true;
}

MasteringDisplayColorVolume::MasteringDisplayColorVolume() = default;
MasteringDisplayColorVolume::MasteringDisplayColorVolume(
    const MasteringDisplayColorVolume& other) = default;
MasteringDisplayColorVolume::~MasteringDisplayColorVolume() = default;

FourCC MasteringDisplayColorVolume::BoxType() const {
  return FOURCC_MDCV;
}

bool MasteringDisplayColorVolume::Parse(BoxReader* reader) {
  // Technically the color coordinates may be in any order.  The spec recommends
  // GBR and it is assumed that the color coordinates are in such order.
  constexpr float kUnitOfMasteringLuminance = 10000;
  RCHECK(ReadMdcvColorCoordinate(reader, &display_primaries_gx) &&
         ReadMdcvColorCoordinate(reader, &display_primaries_gy) &&
         ReadMdcvColorCoordinate(reader, &display_primaries_bx) &&
         ReadMdcvColorCoordinate(reader, &display_primaries_by) &&
         ReadMdcvColorCoordinate(reader, &display_primaries_rx) &&
         ReadMdcvColorCoordinate(reader, &display_primaries_ry) &&
         ReadMdcvColorCoordinate(reader, &white_point_x) &&
         ReadMdcvColorCoordinate(reader, &white_point_y) &&
         ReadFixedPoint32(kUnitOfMasteringLuminance, reader,
                          &max_display_mastering_luminance) &&
         ReadFixedPoint32(kUnitOfMasteringLuminance, reader,
                          &min_display_mastering_luminance));
  return true;
}

FourCC SMPTE2086MasteringDisplayMetadataBox::BoxType() const {
  return FOURCC_SMDM;
}

bool SMPTE2086MasteringDisplayMetadataBox::Parse(BoxReader* reader) {
  constexpr float kColorCoordinateUnit = 1 << 16;
  constexpr float kLuminanceMaxUnit = 1 << 8;
  constexpr float kLuminanceMinUnit = 1 << 14;

  RCHECK(reader->ReadFullBoxHeader());

  // Technically the color coordinates may be in any order.  The spec recommends
  // RGB and it is assumed that the color coordinates are in such order.
  RCHECK(
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_rx) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_ry) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_gx) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_gy) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_bx) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &display_primaries_by) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &white_point_x) &&
      ReadFixedPoint16(kColorCoordinateUnit, reader, &white_point_y) &&
      ReadFixedPoint32(kLuminanceMaxUnit, reader,
                       &max_display_mastering_luminance) &&
      ReadFixedPoint32(kLuminanceMinUnit, reader,
                       &min_display_mastering_luminance));
  return true;
}

ContentLightLevelInformation::ContentLightLevelInformation() = default;
ContentLightLevelInformation::ContentLightLevelInformation(
    const ContentLightLevelInformation& other) = default;
ContentLightLevelInformation::~ContentLightLevelInformation() = default;
FourCC ContentLightLevelInformation::BoxType() const {
  return FOURCC_CLLI;
}

bool ContentLightLevelInformation::Parse(BoxReader* reader) {
  return reader->Read2(&max_content_light_level) &&
         reader->Read2(&max_pic_average_light_level);
}

bool ContentLightLevel::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  return ContentLightLevelInformation::Parse(reader);
}

FourCC ContentLightLevel::BoxType() const {
  return FOURCC_COLL;
}

VideoSampleEntry::VideoSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      width(0),
      height(0),
      alpha_mode(VideoDecoderConfig::AlphaMode::kIsOpaque),
      video_info({VideoCodec::kUnknown, VIDEO_CODEC_PROFILE_UNKNOWN,
                  kNoVideoCodecLevel}) {}

VideoSampleEntry::VideoSampleEntry(const VideoSampleEntry& other) = default;

VideoSampleEntry::~VideoSampleEntry() = default;
FourCC VideoSampleEntry::BoxType() const {
  DCHECK(false) << "VideoSampleEntry should be parsed according to the "
                << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

// static
VideoColorSpace VideoSampleEntry::ConvertColorParameterInformationToColorSpace(
    const ColorParameterInformation& info) {
  // Note that we don't check whether the embedded ids are valid.  We rely on
  // the underlying video decoder to reject any ids that it doesn't support.
  return VideoColorSpace(info.colour_primaries, info.transfer_characteristics,
                         info.matrix_coefficients,
                         info.full_range ? gfx::ColorSpace::RangeID::FULL
                                         : gfx::ColorSpace::RangeID::LIMITED);
}

bool VideoSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(16) &&
         reader->Read2(&width) &&
         reader->Read2(&height) &&
         reader->SkipBytes(50));

  RCHECK(reader->ScanChildren());
  if (reader->HasChild(&pixel_aspect)) {
    RCHECK(reader->MaybeReadChild(&pixel_aspect));
  }

  if (format == FOURCC_ENCV) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (!sinf.HasSupportedScheme()) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  gfx::HDRMetadata hdr_static_metadata;
  const FourCC actual_format =
      format == FOURCC_ENCV ? sinf.format.format : format;
  switch (actual_format) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case FOURCC_AVC1:
    case FOURCC_AVC3: {
      DVLOG(2) << __func__ << " reading AVCDecoderConfigurationRecord (avcC)";
      std::unique_ptr<AVCDecoderConfigurationRecord> avc_config(
          new AVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(avc_config.get()));
      video_info.codec = VideoCodec::kH264;
      video_info.profile = H264Parser::ProfileIDCToVideoCodecProfile(
          avc_config->profile_indication);
      // It can be Dolby Vision stream if there is dvvC box.
      std::tie(video_info, dv_info) = MaybeParseDOVI(reader, video_info);
      frame_bitstream_converter =
          base::MakeRefCounted<AVCBitstreamConverter>(std::move(avc_config));
      break;
    }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case FOURCC_HEV1:
    case FOURCC_HVC1: {
      DVLOG(2) << __func__ << " reading HEVCDecoderConfigurationRecord (hvcC)";
      std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config(
          new HEVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(hevc_config.get()));
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      video_color_space = hevc_config->GetColorSpace();
      hdr_metadata = hevc_config->GetHDRMetadata();
      alpha_mode = hevc_config->GetAlphaMode();
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      video_info.codec = VideoCodec::kHEVC;
      video_info.profile = hevc_config->GetVideoProfile();
      // It can be Dolby Vision stream if there is dvcC/dvvC box.
      std::tie(video_info, dv_info) = MaybeParseDOVI(reader, video_info);
      frame_bitstream_converter =
          base::MakeRefCounted<HEVCBitstreamConverter>(std::move(hevc_config));
      break;
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case FOURCC_DVA1:
    case FOURCC_DVAV: {
      DVLOG(2) << __func__ << " reading AVCDecoderConfigurationRecord (avcC)";
      std::unique_ptr<AVCDecoderConfigurationRecord> avc_config(
          new AVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(avc_config.get()));
      video_info.codec = VideoCodec::kH264;
      video_info.profile = H264Parser::ProfileIDCToVideoCodecProfile(
          avc_config->profile_indication);
      std::tie(video_info, dv_info) = MaybeParseDOVI(reader, video_info);
      frame_bitstream_converter =
          base::MakeRefCounted<AVCBitstreamConverter>(std::move(avc_config));
      break;
    }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case FOURCC_DVH1:
    case FOURCC_DVHE: {
      DVLOG(2) << __func__ << " reading HEVCDecoderConfigurationRecord (hvcC)";
      std::unique_ptr<HEVCDecoderConfigurationRecord> hevc_config(
          new HEVCDecoderConfigurationRecord());
      RCHECK(reader->ReadChild(hevc_config.get()));
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      video_color_space = hevc_config->GetColorSpace();
      hdr_metadata = hevc_config->GetHDRMetadata();
      alpha_mode = hevc_config->GetAlphaMode();
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      video_info.codec = VideoCodec::kHEVC;
      video_info.profile = hevc_config->GetVideoProfile();
      std::tie(video_info, dv_info) = MaybeParseDOVI(reader, video_info);
      frame_bitstream_converter =
          base::MakeRefCounted<HEVCBitstreamConverter>(std::move(hevc_config));
      break;
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    case FOURCC_VP09: {
      DVLOG(2) << __func__ << " reading VPCodecConfigurationRecord (vpcC)";
      std::unique_ptr<VPCodecConfigurationRecord> vp_config(
          new VPCodecConfigurationRecord());
      RCHECK(reader->ReadChild(vp_config.get()));
      video_info.codec = VideoCodec::kVP9;
      video_info.profile = vp_config->profile;
      video_info.level = vp_config->level;
      video_color_space = vp_config->color_space;
      frame_bitstream_converter = nullptr;

      SMPTE2086MasteringDisplayMetadataBox color_volume;
      if (reader->HasChild(&color_volume)) {
        RCHECK(reader->ReadChild(&color_volume));
        hdr_static_metadata.smpte_st_2086 =
            ConvertMdcvToColorVolumeMetadata(color_volume);
      }

      ContentLightLevel level_information;
      if (reader->HasChild(&level_information)) {
        RCHECK(reader->ReadChild(&level_information));
        hdr_static_metadata.cta_861_3 = gfx::HdrMetadataCta861_3(
            level_information.max_content_light_level,
            level_information.max_pic_average_light_level);
      }
      break;
    }
#if BUILDFLAG(ENABLE_AV1_DECODER)
    case FOURCC_AV01: {
      DVLOG(2) << __func__ << " reading AV1CodecConfigurationRecord (av1C)";
      AV1CodecConfigurationRecord av1_config;
      RCHECK(reader->ReadChild(&av1_config));
      video_info.codec = VideoCodec::kAV1;
      video_info.profile = av1_config.profile;
      frame_bitstream_converter = nullptr;
      break;
    }
#endif
    default:
      // Unknown/unsupported format
      MEDIA_LOG(ERROR, reader->media_log())
          << "Unsupported VisualSampleEntry type "
          << FourCCToString(actual_format);
      return false;
  }

  ColorParameterInformation color_parameter_information;
  if (reader->HasChild(&color_parameter_information)) {
    RCHECK(reader->ReadChild(&color_parameter_information));
    if (color_parameter_information.fully_parsed) {
      video_color_space = ConvertColorParameterInformationToColorSpace(
          color_parameter_information);
    }
  }

  MasteringDisplayColorVolume color_volume;
  if (reader->HasChild(&color_volume)) {
    RCHECK(reader->ReadChild(&color_volume));
    hdr_static_metadata.smpte_st_2086 =
        ConvertMdcvToColorVolumeMetadata(color_volume);
  }

  ContentLightLevelInformation level_information;
  if (reader->HasChild(&level_information)) {
    RCHECK(reader->ReadChild(&level_information));
    hdr_static_metadata.cta_861_3 =
        gfx::HdrMetadataCta861_3(level_information.max_content_light_level,
                                 level_information.max_pic_average_light_level);
  }

  if (hdr_static_metadata.IsValid()) {
    hdr_metadata = hdr_static_metadata;
  }

  if (video_info.profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    MEDIA_LOG(ERROR, reader->media_log()) << "Unrecognized video codec profile";
    return false;
  }

  return true;
}

bool VideoSampleEntry::IsFormatValid() const {
  const FourCC actual_format =
      format == FOURCC_ENCV ? sinf.format.format : format;
  switch (actual_format) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case FOURCC_AVC1:
    case FOURCC_AVC3:
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case FOURCC_HEV1:
    case FOURCC_HVC1:
    case FOURCC_DVH1:
    case FOURCC_DVHE:
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case FOURCC_DVA1:
    case FOURCC_DVAV:
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    case FOURCC_VP09:
      return true;
#if BUILDFLAG(ENABLE_AV1_DECODER)
    case FOURCC_AV01:
      return true;
#endif
    default:
      return false;
  }
}

ElementaryStreamDescriptor::ElementaryStreamDescriptor()
    : object_type(kForbidden) {}

ElementaryStreamDescriptor::ElementaryStreamDescriptor(
    const ElementaryStreamDescriptor& other) = default;

ElementaryStreamDescriptor::~ElementaryStreamDescriptor() = default;

FourCC ElementaryStreamDescriptor::BoxType() const {
  return FOURCC_ESDS;
}

bool ElementaryStreamDescriptor::Parse(BoxReader* reader) {
  std::vector<uint8_t> data;
  ESDescriptor es_desc;

  RCHECK(reader->ReadFullBoxHeader());
  RCHECK(reader->ReadVec(&data, reader->box_size() - reader->pos()));
  RCHECK(es_desc.Parse(data));

  object_type = es_desc.object_type();

  if (es_desc.IsAAC(object_type)) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    RCHECK(aac.Parse(es_desc.decoder_specific_info(), reader->media_log()));
#else
    return false;
#endif
  }

  return true;
}

FlacSpecificBox::FlacSpecificBox()
    : sample_rate(0), channel_count(0), bits_per_sample(0) {}

FlacSpecificBox::FlacSpecificBox(const FlacSpecificBox& other) = default;

FlacSpecificBox::~FlacSpecificBox() = default;

FourCC FlacSpecificBox::BoxType() const {
  return FOURCC_DFLA;
}

bool FlacSpecificBox::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  RCHECK_MEDIA_LOGGED(reader->version() == 0, reader->media_log(),
                      "Only version 0 FLACSpecificBox (dfLa) is supported.");
  RCHECK_MEDIA_LOGGED(reader->flags() == 0, reader->media_log(),
                      "Only 0 flags in FLACSpecificBox (dfLa) is supported.");

  // From https://github.com/xiph/flac/blob/master/doc/isoflac.txt, a
  // FLACMetadataBlock is formatted as:
  //   unsigned int(1) LastMetadataBlockFlag;
  //   unsigned int(7) BlockType;
  //   unsigned int(24) Length;
  //   unsigned int(8)  BlockData[Length];
  // We only care about the first block, which must exist, and must be
  // STREAMINFO.
  uint32_t metadata_framing;
  RCHECK_MEDIA_LOGGED(reader->Read4(&metadata_framing), reader->media_log(),
                      "Missing STREAMINFO block in FLACSpecificBox (dfLa).");
  uint8_t block_type = (metadata_framing >> 24) & 0x7f;
  RCHECK_MEDIA_LOGGED(block_type == 0, reader->media_log(),
                      "FLACSpecificBox metadata must begin with STREAMINFO.");
  uint32_t block_length = metadata_framing & 0x00ffffff;
  RCHECK_MEDIA_LOGGED(
      block_length == kFlacMetadataBlockStreaminfoSize, reader->media_log(),
      "STREAMINFO block in FLACSpecificBox (dfLa) has incorrect size.");

  // See https://xiph.org/flac/format.html#metadata_block_streaminfo for
  // STREAMINFO structure format and semantics. We only care about
  // |sample_rate|, |channel_count|,  and |bits_per_sample|,
  // though we also copy the STREAMINFO block for use later in audio decoder
  // configuration. See also the FLAC AudioSampleEntry logic: the |sample_rate|
  // here is used instead of that in the AudioSampleEntry per
  // https://github.com/xiph/flac/blob/master/doc/isoflac.txt.
  RCHECK(reader->ReadVec(&stream_info, kFlacMetadataBlockStreaminfoSize));
  // Bytes 0-9 (min/max block and frame sizes) are ignored here.
  sample_rate = stream_info[10] << 12;
  sample_rate += stream_info[11] << 4;
  sample_rate += (stream_info[12] >> 4) & 0xf;
  RCHECK_MEDIA_LOGGED(sample_rate > 0, reader->media_log(),
                      "STREAMINFO block in FLACSpecificBox (dfLa) must have "
                      "nonzero sample rate.");

  channel_count = (stream_info[12] >> 1) & 0x7;
  channel_count++;

  bits_per_sample = (stream_info[12] & 1) << 4;
  bits_per_sample += (stream_info[13] >> 4) & 0xf;
  bits_per_sample++;

  // The lower 4 bits of byte 13 and all of bytes 14-17 (number of samples in
  // stream) are ignored here.
  // Bytes 18-33 (hash of the unencoded audio data) are ignored here.

  return true;
}

OpusSpecificBox::OpusSpecificBox()
    : seek_preroll(base::Milliseconds(80)), codec_delay_in_frames(0) {}

OpusSpecificBox::OpusSpecificBox(const OpusSpecificBox& other) = default;

OpusSpecificBox::~OpusSpecificBox() = default;

FourCC OpusSpecificBox::BoxType() const {
  return FOURCC_DOPS;
}

bool OpusSpecificBox::Parse(BoxReader* reader) {
  // Extradata must start with "OpusHead" magic.
  extradata.insert(extradata.end(),
                   {0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64});

  // The opus specific box must be present and at least OPUS_EXTRADATA_SIZE - 8
  // bytes in length. The -8 is for the missing "OpusHead" magic signature that
  // is required at the start of the extradata we give to the codec.
  const size_t headerless_extradata_size = reader->box_size() - reader->pos();
  RCHECK(headerless_extradata_size >= OPUS_EXTRADATA_SIZE - extradata.size());
  extradata.resize(extradata.size() + headerless_extradata_size);

  int16_t gain_db;

  RCHECK(reader->Read1(&extradata[OPUS_EXTRADATA_VERSION_OFFSET]));
  RCHECK(reader->Read1(&extradata[OPUS_EXTRADATA_CHANNELS_OFFSET]));
  RCHECK(reader->Read2(&codec_delay_in_frames /* PreSkip */));
  RCHECK(reader->Read4(&sample_rate));
  RCHECK(reader->Read2s(&gain_db));

#if !defined(ARCH_CPU_LITTLE_ENDIAN)
#error The code below assumes little-endianness.
#endif

  memcpy(&extradata[OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET], &codec_delay_in_frames,
         sizeof(codec_delay_in_frames));
  memcpy(&extradata[OPUS_EXTRADATA_SAMPLE_RATE_OFFSET], &sample_rate,
         sizeof(sample_rate));
  memcpy(&extradata[OPUS_EXTRADATA_GAIN_OFFSET], &gain_db, sizeof(gain_db));

  channel_count = extradata[OPUS_EXTRADATA_CHANNELS_OFFSET];

  // Any remaining data is 1-byte data, so copy it over as is, there should
  // only be a handful of these entries, so reading byte by byte is okay.
  for (size_t i = OPUS_EXTRADATA_CHANNEL_MAPPING_OFFSET; i < extradata.size();
       ++i) {
    RCHECK(reader->Read1(&extradata[i]));
  }

  return true;
}

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
DtsSpecificBox::DtsSpecificBox() {}

DtsSpecificBox::DtsSpecificBox(const DtsSpecificBox& other) = default;

DtsSpecificBox::~DtsSpecificBox() = default;

FourCC DtsSpecificBox::BoxType() const {
  return FOURCC_DDTS;
}

bool DtsSpecificBox::Parse(BoxReader* reader) {
  // Read ddts into buffer.
  std::vector<uint8_t> dts_data;

  RCHECK(reader->ReadVec(&dts_data, reader->box_size() - reader->pos()));
  RCHECK(dts.Parse(dts_data, reader->media_log()));

  return true;
}

DtsUhdSpecificBox::DtsUhdSpecificBox() {}

DtsUhdSpecificBox::DtsUhdSpecificBox(const DtsUhdSpecificBox& other) = default;

DtsUhdSpecificBox::~DtsUhdSpecificBox() = default;

FourCC DtsUhdSpecificBox::BoxType() const {
  return FOURCC_UDTS;
}

bool DtsUhdSpecificBox::Parse(BoxReader* reader) {
  // Read udts into buffer.
  std::vector<uint8_t> dtsx_data;

  RCHECK(reader->ReadVec(&dtsx_data, reader->box_size() - reader->pos()));
  RCHECK(dtsx.Parse(dtsx_data, reader->media_log()));

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
EC3SpecificBox::EC3SpecificBox() {}

EC3SpecificBox::EC3SpecificBox(const EC3SpecificBox& other) = default;

EC3SpecificBox::~EC3SpecificBox() = default;

FourCC EC3SpecificBox::BoxType() const {
  return FOURCC_DEC3;
}

bool EC3SpecificBox::Parse(BoxReader* reader) {
  // Read dec3 into buffer.
  std::vector<uint8_t> eac3_data;

  RCHECK(reader->ReadVec(&eac3_data, reader->box_size() - reader->pos()));
  RCHECK(dec3.Parse(eac3_data, reader->media_log()));

  return true;
}

AC3SpecificBox::AC3SpecificBox() {}

AC3SpecificBox::AC3SpecificBox(const AC3SpecificBox& other) = default;

AC3SpecificBox::~AC3SpecificBox() = default;

FourCC AC3SpecificBox::BoxType() const {
  return FOURCC_DAC3;
}

bool AC3SpecificBox::Parse(BoxReader* reader) {
  // Read dac3 into buffer.
  std::vector<uint8_t> ac3_data;

  RCHECK(reader->ReadVec(&ac3_data, reader->box_size() - reader->pos()));
  RCHECK(dac3.Parse(ac3_data, reader->media_log()));

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
AC4SpecificBox::AC4SpecificBox() {}

AC4SpecificBox::AC4SpecificBox(const AC4SpecificBox& other) = default;

AC4SpecificBox::~AC4SpecificBox() = default;

FourCC AC4SpecificBox::BoxType() const {
  return FOURCC_DAC4;
}

bool AC4SpecificBox::Parse(BoxReader* reader) {
  // Read dac4 into buffer.
  std::vector<uint8_t> ac4_data;

  RCHECK(reader->ReadVec(&ac4_data, reader->box_size() - reader->pos()));
  RCHECK(dac4.Parse(ac4_data, reader->media_log()));

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
enum IamfConfigObuType {
  // The following enum values are mapped to their respective Config OBU
  // values in the IAMF specification.
  // https://aomediacodec.github.io/iamf/v1.0.0.html#obu-header-syntax.
  kIamfConfigObuTypeCodecConfig = 0,
  kIamfConfigObuTypeAudioElement = 1,
  kIamfConfigObuTypeMixPresentation = 2,
  kIamfConfigObuTypeSequenceHeader = 31,
};

IamfSpecificBox::IamfSpecificBox() = default;

IamfSpecificBox::IamfSpecificBox(const IamfSpecificBox& other) = default;

IamfSpecificBox::~IamfSpecificBox() = default;

FourCC IamfSpecificBox::BoxType() const {
  return FOURCC_IACB;
}

bool IamfSpecificBox::Parse(BoxReader* reader) {
  uint8_t configuration_version;
  RCHECK(reader->Read1(&configuration_version));
  RCHECK(configuration_version == 0x01);

  uint32_t config_obus_size;
  RCHECK(ReadLeb128Value(reader, &config_obus_size));

  base::span<const uint8_t> buffer =
      reader->buffer().subspan(reader->pos(), config_obus_size);
  ia_descriptors.assign(buffer.begin(), buffer.end());

  RCHECK(reader->SkipBytes(config_obus_size));

  BufferReader config_reader(ia_descriptors.data(), ia_descriptors.size());

  while (config_reader.pos() < config_reader.buffer().size()) {
    RCHECK(ReadOBU(&config_reader));
  }

  return true;
}

bool IamfSpecificBox::ReadOBU(BufferReader* reader) {
  uint8_t obu_type;
  uint32_t obu_size;
  RCHECK(ReadOBUHeader(reader, &obu_type, &obu_size));
  const size_t read_stop_pos = reader->pos() + obu_size;

  switch (static_cast<int>(obu_type)) {
    case kIamfConfigObuTypeCodecConfig:
    case kIamfConfigObuTypeAudioElement:
    case kIamfConfigObuTypeMixPresentation:
      break;
    case kIamfConfigObuTypeSequenceHeader:
      uint32_t ia_code;
      RCHECK(reader->Read4(&ia_code));
      RCHECK(ia_code == FOURCC_IAMF);

      RCHECK(reader->Read1(&profile));
      RCHECK(profile <= 1);

      break;
    default:
      DVLOG(1) << "Unhandled IAMF OBU type " << static_cast<int>(obu_type);
      return false;
  }

  const size_t remaining_size = read_stop_pos - reader->pos();
  RCHECK(reader->SkipBytes(remaining_size));
  return true;
}

bool IamfSpecificBox::ReadOBUHeader(BufferReader* reader,
                                    uint8_t* obu_type,
                                    uint32_t* obu_size) {
  uint8_t header_flags;
  RCHECK(reader->Read1(&header_flags));
  *obu_type = (header_flags >> 3) & 0x1f;

  const bool obu_redundant_copy = (header_flags >> 2) & 1;
  const bool obu_trimming_status_flag = (header_flags >> 1) & 1;
  const bool obu_extension_flag = header_flags & 1;

  redundant_copy |= obu_redundant_copy;

  RCHECK(ReadLeb128Value(reader, obu_size));
  RCHECK(reader->HasBytes(*obu_size));

  RCHECK(!obu_trimming_status_flag);
  if (obu_extension_flag) {
    uint32_t extension_header_size;
    const int last_reader_pos = reader->pos();
    RCHECK(ReadLeb128Value(reader, &extension_header_size));
    const int num_leb128_bytes_read = reader->pos() - last_reader_pos;
    RCHECK(reader->SkipBytes(extension_header_size));
    obu_size -= (num_leb128_bytes_read + extension_header_size);
  }
  return true;
}

bool IamfSpecificBox::ReadLeb128Value(BufferReader* reader,
                                      uint32_t* value) const {
  DCHECK(reader);
  DCHECK(value);
  *value = 0;
  bool error = true;
  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    uint8_t byte;
    RCHECK(reader->Read1(&byte));
    *value |= ((byte & 0x7f) << (i * 7));
    if (!(byte & 0x80)) {
      error = false;
      break;
    }
  }

  return !error;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

AudioSampleEntry::AudioSampleEntry()
    : format(FOURCC_NULL),
      data_reference_index(0),
      channelcount(0),
      samplesize(0),
      samplerate(0) {}

AudioSampleEntry::AudioSampleEntry(const AudioSampleEntry& other) = default;

AudioSampleEntry::~AudioSampleEntry() = default;

FourCC AudioSampleEntry::BoxType() const {
  DCHECK(false) << "AudioSampleEntry should be parsed according to the "
                << "handler type recovered in its Media ancestor.";
  return FOURCC_NULL;
}

bool AudioSampleEntry::Parse(BoxReader* reader) {
  format = reader->type();
  RCHECK(reader->SkipBytes(6) &&
         reader->Read2(&data_reference_index) &&
         reader->SkipBytes(8) &&
         reader->Read2(&channelcount) &&
         reader->Read2(&samplesize) &&
         reader->SkipBytes(4) &&
         reader->Read4(&samplerate));
  // Convert from 16.16 fixed point to integer
  samplerate >>= 16;

  RCHECK(reader->ScanChildren());
  if (format == FOURCC_ENCA) {
    // Continue scanning until a recognized protection scheme is found, or until
    // we run out of protection schemes.
    while (!sinf.HasSupportedScheme()) {
      if (!reader->ReadChild(&sinf))
        return false;
    }
  }

  if (format == FOURCC_OPUS ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_OPUS)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&dops), reader->media_log(),
                        "Failure parsing OpusSpecificBox (dOps)");
    RCHECK_MEDIA_LOGGED(channelcount == dops.channel_count, reader->media_log(),
                        "Opus AudioSampleEntry channel count mismatches "
                        "OpusSpecificBox STREAMINFO channel count");
    RCHECK_MEDIA_LOGGED(samplerate == dops.sample_rate, reader->media_log(),
                        "Opus AudioSampleEntry sample rate mismatches "
                        "OpusSpecificBox STREAMINFO channel count");
  }

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  if (format == FOURCC_DTSC || format == FOURCC_DTSE) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&ddts), reader->media_log(),
                        "Failure parsing DtsSpecificBox (ddts)");
  } else if (format == FOURCC_DTSX) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&udts), reader->media_log(),
                        "Failure parsing DtsUhdSpecificBox (udts)");
    std::bitset<32> ch_bitset(udts.dtsx.GetChannelMask());
    RCHECK_MEDIA_LOGGED(channelcount == ch_bitset.count(), reader->media_log(),
                        "DTSX AudioSampleEntry channel count mismatches "
                        "DtsUhdSpecificBox ChannelMask channel count");
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  if (format == FOURCC_AC3 ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_AC3)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&ac3), reader->media_log(),
                        "Failure parsing AC3SpecificBox (dac3)");
  }
  if (format == FOURCC_EAC3 ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_EAC3)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&eac3), reader->media_log(),
                        "Failure parsing EC3SpecificBox (dec3)");
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  if (format == FOURCC_AC4 ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_AC4)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&ac4), reader->media_log(),
                        "Failure parsing AC4SpecificBox (dac4)");
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

#if BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)
  if (format == FOURCC_IAMF ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_IAMF)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&iacb), reader->media_log(),
                        "Failure parsing IamfSpecificBox (iacb)");
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_IAMF_AUDIO)

  // Read the FLACSpecificBox, even if CENC is signalled.
  if (format == FOURCC_FLAC ||
      (format == FOURCC_ENCA && sinf.format.format == FOURCC_FLAC)) {
    RCHECK_MEDIA_LOGGED(reader->ReadChild(&dfla), reader->media_log(),
                        "Failure parsing FLACSpecificBox (dfLa)");

    // AudioSampleEntry is constrained to max 65535Hz. Instead, use the sample
    // rate from the FlacSpecificBox per
    // https://github.com/xiph/flac/blob/master/doc/isoflac.txt
    if (samplerate != dfla.sample_rate) {
      MEDIA_LOG(INFO, reader->media_log())
          << "FLAC AudioSampleEntry sample rate " << samplerate
          << " overridden by rate " << dfla.sample_rate
          << " from FLACSpecificBox's STREAMINFO metadata";
      samplerate = dfla.sample_rate;
    }

    RCHECK_MEDIA_LOGGED(channelcount == dfla.channel_count, reader->media_log(),
                        "FLAC AudioSampleEntry channel count mismatches "
                        "FLACSpecificBox STREAMINFO channel count");

    RCHECK_MEDIA_LOGGED(samplesize == dfla.bits_per_sample, reader->media_log(),
                        "FLAC AudioSampleEntry sample size mismatches "
                        "FLACSpecificBox STREAMINFO sample size");
  } else {
    RCHECK_MEDIA_LOGGED(!reader->HasChild(&dfla), reader->media_log(),
                        "FLACSpecificBox (dfLa) must only be used with FLAC "
                        "AudioSampleEntry or CENC AudioSampleEntry wrapping "
                        "FLAC");
  }

  // ESDS is not valid in case of EAC3.
  RCHECK(reader->MaybeReadChild(&esds));
  return true;
}

MediaHeader::MediaHeader()
    : creation_time(0),
      modification_time(0),
      timescale(0),
      duration(0),
      language_code(0) {}
MediaHeader::MediaHeader(const MediaHeader& other) = default;
MediaHeader::~MediaHeader() = default;
FourCC MediaHeader::BoxType() const { return FOURCC_MDHD; }

bool MediaHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());

  if (reader->version() == 1) {
    RCHECK(reader->Read8(&creation_time) && reader->Read8(&modification_time) &&
           reader->Read4(&timescale) && reader->Read8(&duration) &&
           reader->Read2(&language_code));
  } else {
    RCHECK(reader->Read4Into8(&creation_time) &&
           reader->Read4Into8(&modification_time) &&
           reader->Read4(&timescale) && reader->Read4Into8(&duration) &&
           reader->Read2(&language_code));
  }

  RCHECK_MEDIA_LOGGED(timescale > 0, reader->media_log(),
                      "Track media header's timescale must not be 0");

  // ISO 639-2/T language code only uses 15 lower bits, so reset the 16th bit.
  language_code &= 0x7fff;
  // Skip playback quality information
  return reader->SkipBytes(2);
}

std::string MediaHeader::language() const {
  if (language_code == 0x7fff || language_code < 0x400) {
    return "und";
  }
  char lang_chars[4];
  lang_chars[3] = 0;
  lang_chars[2] = 0x60 + (language_code & 0x1f);
  lang_chars[1] = 0x60 + ((language_code >> 5) & 0x1f);
  lang_chars[0] = 0x60 + ((language_code >> 10) & 0x1f);

  if (lang_chars[0] < 'a' || lang_chars[0] > 'z' || lang_chars[1] < 'a' ||
      lang_chars[1] > 'z' || lang_chars[2] < 'a' || lang_chars[2] > 'z') {
    // Got unexpected characters in ISO 639-2/T language code. Something must be
    // wrong with the input file, report 'und' language to be safe.
    DVLOG(2) << "Ignoring MDHD language_code (non ISO 639-2 compliant): "
             << lang_chars;
    lang_chars[0] = 'u';
    lang_chars[1] = 'n';
    lang_chars[2] = 'd';
  }

  return lang_chars;
}

MediaInformation::MediaInformation() = default;
MediaInformation::MediaInformation(const MediaInformation& other) = default;
MediaInformation::~MediaInformation() = default;
FourCC MediaInformation::BoxType() const { return FOURCC_MINF; }

bool MediaInformation::Parse(BoxReader* reader) {
  return reader->ScanChildren() &&
         reader->ReadChild(&sample_table);
}

Media::Media() = default;
Media::Media(const Media& other) = default;
Media::~Media() = default;
FourCC Media::BoxType() const { return FOURCC_MDIA; }

bool Media::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&handler));

  // Maddeningly, the HandlerReference box specifies how to parse the
  // SampleDescription box, making the latter the only box (of those that we
  // support) which cannot be parsed correctly on its own (or even with
  // information from its strict ancestor tree). We thus copy the handler type
  // to the sample description box *before* parsing it to provide this
  // information while parsing.
  information.sample_table.description.type = handler.type;
  RCHECK(reader->ReadChild(&information));
  return true;
}

Track::Track() = default;
Track::Track(const Track& other) = default;
Track::~Track() = default;
FourCC Track::BoxType() const { return FOURCC_TRAK; }

bool Track::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChild(&media) &&
         reader->MaybeReadChild(&edit));
  return true;
}

MovieExtendsHeader::MovieExtendsHeader() : fragment_duration(0) {}
MovieExtendsHeader::MovieExtendsHeader(const MovieExtendsHeader& other) =
    default;
MovieExtendsHeader::~MovieExtendsHeader() = default;
FourCC MovieExtendsHeader::BoxType() const { return FOURCC_MEHD; }

bool MovieExtendsHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1) {
    RCHECK(reader->Read8(&fragment_duration));
  } else {
    RCHECK(reader->Read4Into8(&fragment_duration));
  }
  return true;
}

TrackExtends::TrackExtends()
    : track_id(0),
      default_sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0) {}
TrackExtends::TrackExtends(const TrackExtends& other) = default;
TrackExtends::~TrackExtends() = default;
FourCC TrackExtends::BoxType() const { return FOURCC_TREX; }

bool TrackExtends::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&track_id) &&
         reader->Read4(&default_sample_description_index) &&
         reader->Read4(&default_sample_duration) &&
         reader->Read4(&default_sample_size) &&
         reader->Read4(&default_sample_flags));
  return true;
}

MovieExtends::MovieExtends() = default;
MovieExtends::MovieExtends(const MovieExtends& other) = default;
MovieExtends::~MovieExtends() = default;
FourCC MovieExtends::BoxType() const { return FOURCC_MVEX; }

bool MovieExtends::Parse(BoxReader* reader) {
  header.fragment_duration = 0;
  return reader->ScanChildren() &&
         reader->MaybeReadChild(&header) &&
         reader->ReadChildren(&tracks);
}

Movie::Movie() : fragmented(false) {}
Movie::Movie(const Movie& other) = default;
Movie::~Movie() = default;
FourCC Movie::BoxType() const { return FOURCC_MOOV; }

bool Movie::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() && reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks));

  RCHECK_MEDIA_LOGGED(reader->ReadChild(&extends), reader->media_log(),
                      "Detected unfragmented MP4. Media Source Extensions "
                      "require ISO BMFF moov to contain mvex to indicate that "
                      "Movie Fragments are to be expected.");

  return reader->MaybeReadChildren(&pssh);
}

TrackFragmentDecodeTime::TrackFragmentDecodeTime() : decode_time(0) {}
TrackFragmentDecodeTime::TrackFragmentDecodeTime(
    const TrackFragmentDecodeTime& other) = default;
TrackFragmentDecodeTime::~TrackFragmentDecodeTime() = default;
FourCC TrackFragmentDecodeTime::BoxType() const { return FOURCC_TFDT; }

bool TrackFragmentDecodeTime::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  if (reader->version() == 1)
    return reader->Read8(&decode_time);
  else
    return reader->Read4Into8(&decode_time);
}

MovieFragmentHeader::MovieFragmentHeader() : sequence_number(0) {}
MovieFragmentHeader::MovieFragmentHeader(const MovieFragmentHeader& other) =
    default;
MovieFragmentHeader::~MovieFragmentHeader() = default;
FourCC MovieFragmentHeader::BoxType() const { return FOURCC_MFHD; }

bool MovieFragmentHeader::Parse(BoxReader* reader) {
  return reader->SkipBytes(4) && reader->Read4(&sequence_number);
}

TrackFragmentHeader::TrackFragmentHeader()
    : track_id(0),
      sample_description_index(0),
      default_sample_duration(0),
      default_sample_size(0),
      default_sample_flags(0),
      has_default_sample_flags(false) {}

TrackFragmentHeader::TrackFragmentHeader(const TrackFragmentHeader& other) =
    default;
TrackFragmentHeader::~TrackFragmentHeader() = default;
FourCC TrackFragmentHeader::BoxType() const { return FOURCC_TFHD; }

bool TrackFragmentHeader::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() && reader->Read4(&track_id));

  // Media Source specific: reject tracks that set 'base-data-offset-present'.
  // Although the Media Source requires that 'default-base-is-moof' (14496-12
  // Amendment 2) be set, we omit this check as many otherwise-valid files in
  // the wild don't set it.
  //
  //  RCHECK((flags & 0x020000) && !(flags & 0x1));
  RCHECK_MEDIA_LOGGED(!(reader->flags() & 0x1), reader->media_log(),
                      "TFHD base-data-offset not allowed by MSE. See "
                      "https://www.w3.org/TR/mse-byte-stream-format-isobmff/"
                      "#movie-fragment-relative-addressing");

  if (reader->flags() & 0x2) {
    RCHECK(reader->Read4(&sample_description_index));
  } else {
    sample_description_index = 0;
  }

  if (reader->flags() & 0x8) {
    RCHECK(reader->Read4(&default_sample_duration));
  } else {
    default_sample_duration = 0;
  }

  if (reader->flags() & 0x10) {
    RCHECK(reader->Read4(&default_sample_size));
  } else {
    default_sample_size = 0;
  }

  if (reader->flags() & 0x20) {
    RCHECK(reader->Read4(&default_sample_flags));
    has_default_sample_flags = true;
  } else {
    has_default_sample_flags = false;
  }

  return true;
}

TrackFragmentRun::TrackFragmentRun()
    : sample_count(0), data_offset(0) {}
TrackFragmentRun::TrackFragmentRun(const TrackFragmentRun& other) = default;
TrackFragmentRun::~TrackFragmentRun() = default;
FourCC TrackFragmentRun::BoxType() const { return FOURCC_TRUN; }

bool TrackFragmentRun::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&sample_count));
  const uint32_t flags = reader->flags();

  bool data_offset_present = (flags & 0x1) != 0;
  bool first_sample_flags_present = (flags & 0x4) != 0;
  bool sample_duration_present = (flags & 0x100) != 0;
  bool sample_size_present = (flags & 0x200) != 0;
  bool sample_flags_present = (flags & 0x400) != 0;
  bool sample_composition_time_offsets_present = (flags & 0x800) != 0;

  if (data_offset_present) {
    RCHECK(reader->Read4(&data_offset));
  } else {
    data_offset = 0;
  }

  uint32_t first_sample_flags = 0;
  if (first_sample_flags_present)
    RCHECK(reader->Read4(&first_sample_flags));

  int fields = sample_duration_present + sample_size_present +
      sample_flags_present + sample_composition_time_offsets_present;
  const size_t bytes_per_field = 4;

  // Cast |sample_count| to size_t before multiplying to support maximum
  // platform size.
  base::CheckedNumeric<size_t> bytes_needed = base::CheckMul(
      fields, bytes_per_field, static_cast<size_t>(sample_count));
  RCHECK_MEDIA_LOGGED(
      bytes_needed.IsValid(), reader->media_log(),
      "Extreme TRUN sample count exceeds implementation limit.");
  RCHECK(reader->HasBytes(bytes_needed.ValueOrDie()));

  if (sample_duration_present) {
    RCHECK(sample_count <= sample_durations.max_size());
    sample_durations.resize(sample_count);
  }
  if (sample_size_present) {
    RCHECK(sample_count <= sample_sizes.max_size());
    sample_sizes.resize(sample_count);
  }
  if (sample_flags_present) {
    RCHECK(sample_count <= sample_flags.max_size());
    sample_flags.resize(sample_count);
  }
  if (sample_composition_time_offsets_present) {
    RCHECK(sample_count <= sample_composition_time_offsets.max_size());
    sample_composition_time_offsets.resize(sample_count);
  }

  if (sample_duration_present || sample_size_present || sample_flags_present ||
      sample_composition_time_offsets_present) {
    for (uint32_t i = 0; i < sample_count; ++i) {
      if (sample_duration_present)
        RCHECK(reader->Read4(&sample_durations[i]));
      if (sample_size_present)
        RCHECK(reader->Read4(&sample_sizes[i]));
      if (sample_flags_present)
        RCHECK(reader->Read4(&sample_flags[i]));
      if (sample_composition_time_offsets_present)
        RCHECK(reader->Read4s(&sample_composition_time_offsets[i]));
    }
  }

  if (first_sample_flags_present) {
    if (sample_flags.size() == 0) {
      sample_flags.push_back(first_sample_flags);
    } else {
      sample_flags[0] = first_sample_flags;
    }
  }
  return true;
}

SampleToGroup::SampleToGroup() : grouping_type(0), grouping_type_parameter(0) {}
SampleToGroup::SampleToGroup(const SampleToGroup& other) = default;
SampleToGroup::~SampleToGroup() = default;
FourCC SampleToGroup::BoxType() const { return FOURCC_SBGP; }

bool SampleToGroup::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&grouping_type));

  if (reader->version() == 1)
    RCHECK(reader->Read4(&grouping_type_parameter));

  if (grouping_type != FOURCC_SEIG) {
    DLOG(WARNING) << "SampleToGroup box with grouping_type '" << grouping_type
                  << "' is not supported.";
    return true;
  }

  uint32_t count;
  RCHECK(reader->Read4(&count));

  const size_t bytes_per_entry = 8;
  // Cast |count| to size_t before multiplying to support maximum platform size.
  base::CheckedNumeric<size_t> bytes_needed =
      base::CheckMul(bytes_per_entry, static_cast<size_t>(count));
  RCHECK_MEDIA_LOGGED(bytes_needed.IsValid(), reader->media_log(),
                      "Extreme SBGP count exceeds implementation limit.");
  RCHECK(reader->HasBytes(bytes_needed.ValueOrDie()));

  RCHECK(count <= entries.max_size());
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    RCHECK(reader->Read4(&entries[i].sample_count) &&
           reader->Read4(&entries[i].group_description_index));
  }
  return true;
}

CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry()
    : is_encrypted(false),
      iv_size(0),
      crypt_byte_block(0),
      skip_byte_block(0),
      constant_iv_size(0) {}
CencSampleEncryptionInfoEntry::CencSampleEncryptionInfoEntry(
    const CencSampleEncryptionInfoEntry& other) = default;
CencSampleEncryptionInfoEntry::~CencSampleEncryptionInfoEntry() = default;

bool CencSampleEncryptionInfoEntry::Parse(BoxReader* reader) {
  uint8_t flag;
  uint8_t possible_pattern_info;
  RCHECK(reader->SkipBytes(1) &&  // reserved.
         reader->Read1(&possible_pattern_info) && reader->Read1(&flag) &&
         reader->Read1(&iv_size) && reader->ReadVec(&key_id, kKeyIdSize));

  is_encrypted = (flag != 0);
  if (is_encrypted) {
    crypt_byte_block = (possible_pattern_info >> 4) & 0x0f;
    skip_byte_block = possible_pattern_info & 0x0f;
    if (iv_size == 0) {
      RCHECK(reader->Read1(&constant_iv_size));
      RCHECK(constant_iv_size == 8 || constant_iv_size == 16);
      memset(constant_iv, 0, sizeof(constant_iv));
      for (uint8_t i = 0; i < constant_iv_size; i++)
        RCHECK(reader->Read1(constant_iv + i));
    } else {
      RCHECK(iv_size == 8 || iv_size == 16);
    }
  } else {
    RCHECK(iv_size == 0);
  }
  return true;
}

SampleGroupDescription::SampleGroupDescription() : grouping_type(0) {}
SampleGroupDescription::SampleGroupDescription(
    const SampleGroupDescription& other) = default;
SampleGroupDescription::~SampleGroupDescription() = default;
FourCC SampleGroupDescription::BoxType() const { return FOURCC_SGPD; }

bool SampleGroupDescription::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader() &&
         reader->Read4(&grouping_type));

  if (grouping_type != FOURCC_SEIG) {
    DLOG(WARNING) << "SampleGroupDescription box with grouping_type '"
                  << grouping_type << "' is not supported.";
    return true;
  }

  const uint8_t version = reader->version();

  const size_t kEntrySize = sizeof(uint32_t) + kKeyIdSize;
  uint32_t default_length = 0;
  if (version == 1) {
      RCHECK(reader->Read4(&default_length));
      RCHECK(default_length == 0 || default_length >= kEntrySize);
  }

  uint32_t count;
  RCHECK(reader->Read4(&count));

  // Check that we have at least two bytes for each entry before allocating a
  // potentially huge entries vector. In reality each entry will require a
  // variable number of bytes in excess of 2.
  const int bytes_per_entry = 2;
  // Cast |count| to size_t before multiplying to support maximum platform size.
  base::CheckedNumeric<size_t> bytes_needed =
      base::CheckMul(bytes_per_entry, static_cast<size_t>(count));
  RCHECK_MEDIA_LOGGED(bytes_needed.IsValid(), reader->media_log(),
                      "Extreme SGPD count exceeds implementation limit.");
  RCHECK(reader->HasBytes(bytes_needed.ValueOrDie()));

  RCHECK(count <= entries.max_size());
  entries.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    if (version == 1) {
      if (default_length == 0) {
        uint32_t description_length = 0;
        RCHECK(reader->Read4(&description_length));
        RCHECK(description_length >= kEntrySize);
      }
    }
    RCHECK(entries[i].Parse(reader));
  }
  return true;
}

TrackFragment::TrackFragment() = default;
TrackFragment::TrackFragment(const TrackFragment& other) = default;
TrackFragment::~TrackFragment() = default;
FourCC TrackFragment::BoxType() const { return FOURCC_TRAF; }

bool TrackFragment::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() && reader->ReadChild(&header) &&
         // Media Source specific: 'tfdt' required
         reader->ReadChild(&decode_time) && reader->MaybeReadChildren(&runs) &&
         reader->MaybeReadChild(&auxiliary_offset) &&
         reader->MaybeReadChild(&auxiliary_size) &&
         reader->MaybeReadChild(&sdtp) &&
         reader->MaybeReadChild(&sample_encryption));

  // There could be multiple SampleGroupDescription and SampleToGroup boxes with
  // different grouping types. For common encryption, the relevant grouping type
  // is 'seig'. Continue reading until 'seig' is found, or until running out of
  // child boxes.
  while (reader->HasChild(&sample_group_description)) {
    RCHECK(reader->ReadChild(&sample_group_description));
    if (sample_group_description.grouping_type == FOURCC_SEIG)
      break;
    sample_group_description.entries.clear();
  }
  while (reader->HasChild(&sample_to_group)) {
    RCHECK(reader->ReadChild(&sample_to_group));
    if (sample_to_group.grouping_type == FOURCC_SEIG)
      break;
    sample_to_group.entries.clear();
  }
  return true;
}

MovieFragment::MovieFragment() = default;
MovieFragment::MovieFragment(const MovieFragment& other) = default;
MovieFragment::~MovieFragment() = default;
FourCC MovieFragment::BoxType() const { return FOURCC_MOOF; }

bool MovieFragment::Parse(BoxReader* reader) {
  RCHECK(reader->ScanChildren() &&
         reader->ReadChild(&header) &&
         reader->ReadChildren(&tracks) &&
         reader->MaybeReadChildren(&pssh));
  return true;
}

IndependentAndDisposableSamples::IndependentAndDisposableSamples() = default;
IndependentAndDisposableSamples::IndependentAndDisposableSamples(
    const IndependentAndDisposableSamples& other) = default;
IndependentAndDisposableSamples::~IndependentAndDisposableSamples() = default;
FourCC IndependentAndDisposableSamples::BoxType() const { return FOURCC_SDTP; }

bool IndependentAndDisposableSamples::Parse(BoxReader* reader) {
  RCHECK(reader->ReadFullBoxHeader());
  RCHECK(reader->version() == 0);
  RCHECK(reader->flags() == 0);

  size_t sample_count = reader->box_size() - reader->pos();
  RCHECK(sample_count <= sample_depends_on_.max_size());
  sample_depends_on_.resize(sample_count);
  for (size_t i = 0; i < sample_count; ++i) {
    uint8_t sample_info;
    RCHECK(reader->Read1(&sample_info));

    sample_depends_on_[i] =
        static_cast<SampleDependsOn>((sample_info >> 4) & 0x3);

    RCHECK(sample_depends_on_[i] != kSampleDependsOnReserved);
  }

  return true;
}

SampleDependsOn IndependentAndDisposableSamples::sample_depends_on(
    size_t i) const {
  if (i >= sample_depends_on_.size())
    return kSampleDependsOnUnknown;

  return sample_depends_on_[i];
}

}  // namespace mp4
}  // namespace media
