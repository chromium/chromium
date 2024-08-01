// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_parser.h"

// This file contains code to parse WebM file elements. It was created
// from information in the Matroska spec.
// http://www.matroska.org/technical/specs/index.html
//
// WebM Container Guidelines is at https://www.webmproject.org/docs/container/
// WebM Encryption spec is at: https://www.webmproject.org/docs/webm-encryption/

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

enum ElementType {
  UNKNOWN,
  // The following are basic types defined in the Matroska spec.
  LIST,  // Referred to as Master Element in the Matroska spec.
  UINT,
  FLOAT,
  BINARY,
  STRING,
  // Valid element but we don't care about them right now.
  SKIP,
  // Aliases of SKIP to help keep type info.
  SKIP_LIST = SKIP,
  SKIP_UINT = SKIP,
  SKIP_FLOAT = SKIP,
  SKIP_BINARY = SKIP,
  SKIP_STRING = SKIP,
};

struct ElementIdInfo {
  ElementType type_;
  int id_;
};

struct ListElementInfo {
  int id_;
  int level_;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #reinterpret-cast-trivial-type, #global-scope
  RAW_PTR_EXCLUSION const ElementIdInfo* id_info_;
  int id_info_count_;
};

// The following are tables indicating what IDs are valid sub-elements of
// particular elements. If an element is encountered that doesn't appear in the
// list, a parsing error is signalled. Elements supported by Matroska but not
// supported by WebM are marked with SKIP_* types so that they will be skipped
// but will not fail the parser.

static const ElementIdInfo kEBMLHeaderIds[] = {
    {UINT, kWebMIdEBMLVersion},        {UINT, kWebMIdEBMLReadVersion},
    {UINT, kWebMIdEBMLMaxIDLength},    {UINT, kWebMIdEBMLMaxSizeLength},
    {STRING, kWebMIdDocType},          {UINT, kWebMIdDocTypeVersion},
    {UINT, kWebMIdDocTypeReadVersion},
};

static const ElementIdInfo kSegmentIds[] = {
    {LIST, kWebMIdSeekHead}, {LIST, kWebMIdInfo},
    {LIST, kWebMIdCluster},  {LIST, kWebMIdTracks},
    {LIST, kWebMIdCues},     {SKIP_LIST, kWebMIdAttachments},
    {LIST, kWebMIdChapters}, {LIST, kWebMIdTags},
};

static const ElementIdInfo kSeekHeadIds[] = {
    {LIST, kWebMIdSeek},
};

static const ElementIdInfo kSeekIds[] = {
    {BINARY, kWebMIdSeekID},
    {UINT, kWebMIdSeekPosition},
};

static const ElementIdInfo kInfoIds[] = {
    {SKIP_BINARY, kWebMIdSegmentUID},
    {SKIP_STRING, kWebMIdSegmentFilename},
    {SKIP_BINARY, kWebMIdPrevUID},
    {SKIP_STRING, kWebMIdPrevFilename},
    {SKIP_BINARY, kWebMIdNextUID},
    {SKIP_STRING, kWebMIdNextFilename},
    {SKIP_BINARY, kWebMIdSegmentFamily},
    {SKIP_LIST, kWebMIdChapterTranslate},
    {UINT, kWebMIdTimecodeScale},
    {FLOAT, kWebMIdDuration},
    {BINARY, kWebMIdDateUTC},
    {STRING, kWebMIdTitle},
    {STRING, kWebMIdMuxingApp},
    {STRING, kWebMIdWritingApp},
};

static const ElementIdInfo kChapterTranslateIds[] = {
    {SKIP_UINT, kWebMIdChapterTranslateEditionUID},
    {SKIP_UINT, kWebMIdChapterTranslateCodec},
    {SKIP_BINARY, kWebMIdChapterTranslateID},
};

static const ElementIdInfo kClusterIds[] = {
    {BINARY, kWebMIdSimpleBlock},     {UINT, kWebMIdTimecode},
    {SKIP_LIST, kWebMIdSilentTracks}, {SKIP_UINT, kWebMIdPosition},
    {UINT, kWebMIdPrevSize},          {LIST, kWebMIdBlockGroup},
};

static const ElementIdInfo kSilentTracksIds[] = {
    {SKIP_UINT, kWebMIdSilentTrackNumber},
};

static const ElementIdInfo kBlockGroupIds[] = {
    {BINARY, kWebMIdBlock},          {LIST, kWebMIdBlockAdditions},
    {UINT, kWebMIdBlockDuration},    {SKIP_UINT, kWebMIdReferencePriority},
    {BINARY, kWebMIdReferenceBlock}, {SKIP_BINARY, kWebMIdCodecState},
    {BINARY, kWebMIdDiscardPadding}, {SKIP_LIST, kWebMIdSlices},
};

static const ElementIdInfo kBlockAdditionsIds[] = {
    {LIST, kWebMIdBlockMore},
};

static const ElementIdInfo kBlockMoreIds[] = {
    {UINT, kWebMIdBlockAddID},
    {BINARY, kWebMIdBlockAdditional},
};

static const ElementIdInfo kSlicesIds[] = {
    {SKIP_LIST, kWebMIdTimeSlice},
};

static const ElementIdInfo kTimeSliceIds[] = {
    {SKIP_UINT, kWebMIdLaceNumber},
};

static const ElementIdInfo kTracksIds[] = {
    {LIST, kWebMIdTrackEntry},
};

static const ElementIdInfo kTrackEntryIds[] = {
    {UINT, kWebMIdTrackNumber},
    {BINARY, kWebMIdTrackUID},
    {UINT, kWebMIdTrackType},
    {UINT, kWebMIdFlagEnabled},
    {UINT, kWebMIdFlagDefault},
    {UINT, kWebMIdFlagForced},
    {UINT, kWebMIdFlagLacing},
    {SKIP_UINT, kWebMIdMinCache},
    {SKIP_UINT, kWebMIdMaxCache},
    {UINT, kWebMIdDefaultDuration},
    {SKIP_FLOAT, kWebMIdTrackTimecodeScale},
    {SKIP_UINT, kWebMIdMaxBlockAdditionId},
    {STRING, kWebMIdName},
    {STRING, kWebMIdLanguage},
    {STRING, kWebMIdCodecID},
    {BINARY, kWebMIdCodecPrivate},
    {STRING, kWebMIdCodecName},
    {SKIP_UINT, kWebMIdAttachmentLink},
    {SKIP_UINT, kWebMIdCodecDecodeAll},
    {SKIP_UINT, kWebMIdTrackOverlay},
    {UINT, kWebMIdCodecDelay},
    {UINT, kWebMIdSeekPreRoll},
    {SKIP_LIST, kWebMIdTrackTranslate},
    {LIST, kWebMIdVideo},
    {LIST, kWebMIdAudio},
    {SKIP_LIST, kWebMIdTrackOperation},
    {LIST, kWebMIdContentEncodings},
};

static const ElementIdInfo kTrackTranslateIds[] = {
    {SKIP_UINT, kWebMIdTrackTranslateEditionUID},
    {SKIP_UINT, kWebMIdTrackTranslateCodec},
    {SKIP_BINARY, kWebMIdTrackTranslateTrackID},
};

static const ElementIdInfo kVideoIds[] = {
    {UINT, kWebMIdFlagInterlaced},  {UINT, kWebMIdStereoMode},
    {UINT, kWebMIdAlphaMode},       {UINT, kWebMIdPixelWidth},
    {UINT, kWebMIdPixelHeight},     {UINT, kWebMIdPixelCropBottom},
    {UINT, kWebMIdPixelCropTop},    {UINT, kWebMIdPixelCropLeft},
    {UINT, kWebMIdPixelCropRight},  {UINT, kWebMIdDisplayWidth},
    {UINT, kWebMIdDisplayHeight},   {UINT, kWebMIdDisplayUnit},
    {UINT, kWebMIdAspectRatioType}, {SKIP_BINARY, kWebMIdColorSpace},
    {SKIP_FLOAT, kWebMIdFrameRate}, {LIST, kWebMIdColour},
    {LIST, kWebMIdProjection},
};

static const ElementIdInfo kColourIds[] = {
    {UINT, kWebMIdMatrixCoefficients},
    {UINT, kWebMIdBitsPerChannel},
    {UINT, kWebMIdChromaSubsamplingHorz},
    {UINT, kWebMIdChromaSubsamplingVert},
    {UINT, kWebMIdCbSubsamplingHorz},
    {UINT, kWebMIdCbSubsamplingVert},
    {UINT, kWebMIdChromaSitingHorz},
    {UINT, kWebMIdChromaSitingVert},
    {UINT, kWebMIdRange},
    {UINT, kWebMIdTransferCharacteristics},
    {UINT, kWebMIdPrimaries},
    {UINT, kWebMIdMaxCLL},
    {UINT, kWebMIdMaxFALL},
    {LIST, kWebMIdColorVolumeMetadata},
};

static const ElementIdInfo kColorVolumeMetadataIds[] = {
    {FLOAT, kWebMIdPrimaryRChromaticityX},
    {FLOAT, kWebMIdPrimaryRChromaticityY},
    {FLOAT, kWebMIdPrimaryGChromaticityX},
    {FLOAT, kWebMIdPrimaryGChromaticityY},
    {FLOAT, kWebMIdPrimaryBChromaticityX},
    {FLOAT, kWebMIdPrimaryBChromaticityY},
    {FLOAT, kWebMIdWhitePointChromaticityX},
    {FLOAT, kWebMIdWhitePointChromaticityY},
    {FLOAT, kWebMIdLuminanceMax},
    {FLOAT, kWebMIdLuminanceMin},
};

static const ElementIdInfo kProjectionIds[]{
    {UINT, kWebMIdProjectionType},      {SKIP_BINARY, kWebMIdProjectionPrivate},
    {FLOAT, kWebMIdProjectionPoseYaw},  {FLOAT, kWebMIdProjectionPosePitch},
    {FLOAT, kWebMIdProjectionPoseRoll},
};

static const ElementIdInfo kAudioIds[] = {
    {FLOAT, kWebMIdSamplingFrequency},
    {FLOAT, kWebMIdOutputSamplingFrequency},
    {UINT, kWebMIdChannels},
    {UINT, kWebMIdBitDepth},
};

static const ElementIdInfo kTrackOperationIds[] = {
    {SKIP_LIST, kWebMIdTrackCombinePlanes},
    {SKIP_LIST, kWebMIdJoinBlocks},
};

static const ElementIdInfo kTrackCombinePlanesIds[] = {
    {SKIP_LIST, kWebMIdTrackPlane},
};

static const ElementIdInfo kTrackPlaneIds[] = {
    {SKIP_UINT, kWebMIdTrackPlaneUID},
    {SKIP_UINT, kWebMIdTrackPlaneType},
};

static const ElementIdInfo kJoinBlocksIds[] = {
    {SKIP_UINT, kWebMIdTrackJoinUID},
};

static const ElementIdInfo kContentEncodingsIds[] = {
    {LIST, kWebMIdContentEncoding},
};

static const ElementIdInfo kContentEncodingIds[] = {
    {UINT, kWebMIdContentEncodingOrder}, {UINT, kWebMIdContentEncodingScope},
    {UINT, kWebMIdContentEncodingType},  {SKIP_LIST, kWebMIdContentCompression},
    {LIST, kWebMIdContentEncryption},
};

static const ElementIdInfo kContentCompressionIds[] = {
    {SKIP_UINT, kWebMIdContentCompAlgo},
    {SKIP_BINARY, kWebMIdContentCompSettings},
};

static const ElementIdInfo kContentEncryptionIds[] = {
    {LIST, kWebMIdContentEncAESSettings},
    {UINT, kWebMIdContentEncAlgo},
    {BINARY, kWebMIdContentEncKeyID},
    {SKIP_BINARY, kWebMIdContentSignature},
    {SKIP_BINARY, kWebMIdContentSigKeyID},
    {SKIP_UINT, kWebMIdContentSigAlgo},
    {SKIP_UINT, kWebMIdContentSigHashAlgo},
};

static const ElementIdInfo kContentEncAESSettingsIds[] = {
    {UINT, kWebMIdAESSettingsCipherMode},
};

static const ElementIdInfo kCuesIds[] = {
    {LIST, kWebMIdCuePoint},
};

static const ElementIdInfo kCuePointIds[] = {
    {UINT, kWebMIdCueTime},
    {LIST, kWebMIdCueTrackPositions},
};

static const ElementIdInfo kCueTrackPositionsIds[] = {
    {UINT, kWebMIdCueTrack},          {UINT, kWebMIdCueClusterPosition},
    {UINT, kWebMIdCueBlockNumber},    {SKIP_UINT, kWebMIdCueCodecState},
    {SKIP_LIST, kWebMIdCueReference},
};

static const ElementIdInfo kCueReferenceIds[] = {
    {SKIP_UINT, kWebMIdCueRefTime},
};

static const ElementIdInfo kAttachmentsIds[] = {
    {SKIP_LIST, kWebMIdAttachedFile},
};

static const ElementIdInfo kAttachedFileIds[] = {
    {SKIP_STRING, kWebMIdFileDescription}, {SKIP_STRING, kWebMIdFileName},
    {SKIP_STRING, kWebMIdFileMimeType},    {SKIP_BINARY, kWebMIdFileData},
    {SKIP_UINT, kWebMIdFileUID},
};

static const ElementIdInfo kChaptersIds[] = {
    {LIST, kWebMIdEditionEntry},
};

static const ElementIdInfo kEditionEntryIds[] = {
    {SKIP_UINT, kWebMIdEditionUID},
    {SKIP_UINT, kWebMIdEditionFlagHidden},
    {SKIP_UINT, kWebMIdEditionFlagDefault},
    {SKIP_UINT, kWebMIdEditionFlagOrdered},
    {LIST, kWebMIdChapterAtom},
};

static const ElementIdInfo kChapterAtomIds[] = {
    {UINT, kWebMIdChapterUID},
    {UINT, kWebMIdChapterTimeStart},
    {UINT, kWebMIdChapterTimeEnd},
    {SKIP_UINT, kWebMIdChapterFlagHidden},
    {SKIP_UINT, kWebMIdChapterFlagEnabled},
    {SKIP_BINARY, kWebMIdChapterSegmentUID},
    {SKIP_UINT, kWebMIdChapterSegmentEditionUID},
    {SKIP_UINT, kWebMIdChapterPhysicalEquiv},
    {SKIP_LIST, kWebMIdChapterTrack},
    {LIST, kWebMIdChapterDisplay},
    {SKIP_LIST, kWebMIdChapProcess},
};

static const ElementIdInfo kChapterTrackIds[] = {
    {SKIP_UINT, kWebMIdChapterTrackNumber},
};

static const ElementIdInfo kChapterDisplayIds[] = {
    {STRING, kWebMIdChapString},
    {STRING, kWebMIdChapLanguage},
    {STRING, kWebMIdChapCountry},
};

static const ElementIdInfo kChapProcessIds[] = {
    {SKIP_UINT, kWebMIdChapProcessCodecID},
    {SKIP_BINARY, kWebMIdChapProcessPrivate},
    {SKIP_LIST, kWebMIdChapProcessCommand},
};

static const ElementIdInfo kChapProcessCommandIds[] = {
    {SKIP_UINT, kWebMIdChapProcessTime},
    {SKIP_BINARY, kWebMIdChapProcessData},
};

static const ElementIdInfo kTagsIds[] = {
    {LIST, kWebMIdTag},
};

static const ElementIdInfo kTagIds[] = {
    {LIST, kWebMIdTargets},
    {LIST, kWebMIdSimpleTag},
};

static const ElementIdInfo kTargetsIds[] = {
    {UINT, kWebMIdTargetTypeValue},    {STRING, kWebMIdTargetType},
    {UINT, kWebMIdTagTrackUID},        {SKIP_UINT, kWebMIdTagEditionUID},
    {SKIP_UINT, kWebMIdTagChapterUID}, {SKIP_UINT, kWebMIdTagAttachmentUID},
};

static const ElementIdInfo kSimpleTagIds[] = {
    {STRING, kWebMIdTagName},   {STRING, kWebMIdTagLanguage},
    {UINT, kWebMIdTagDefault},  {STRING, kWebMIdTagString},
    {BINARY, kWebMIdTagBinary},
};

#define LIST_ELEMENT_INFO(id, level, id_info) \
  { (id), (level), (id_info), std::size(id_info) }

static const ListElementInfo kListElementInfo[] = {
    LIST_ELEMENT_INFO(kWebMIdCluster, 1, kClusterIds),
    LIST_ELEMENT_INFO(kWebMIdEBMLHeader, 0, kEBMLHeaderIds),
    LIST_ELEMENT_INFO(kWebMIdSegment, 0, kSegmentIds),
    LIST_ELEMENT_INFO(kWebMIdSeekHead, 1, kSeekHeadIds),
    LIST_ELEMENT_INFO(kWebMIdSeek, 2, kSeekIds),
    LIST_ELEMENT_INFO(kWebMIdInfo, 1, kInfoIds),
    LIST_ELEMENT_INFO(kWebMIdChapterTranslate, 2, kChapterTranslateIds),
    LIST_ELEMENT_INFO(kWebMIdSilentTracks, 2, kSilentTracksIds),
    LIST_ELEMENT_INFO(kWebMIdBlockGroup, 2, kBlockGroupIds),
    LIST_ELEMENT_INFO(kWebMIdBlockAdditions, 3, kBlockAdditionsIds),
    LIST_ELEMENT_INFO(kWebMIdBlockMore, 4, kBlockMoreIds),
    LIST_ELEMENT_INFO(kWebMIdSlices, 3, kSlicesIds),
    LIST_ELEMENT_INFO(kWebMIdTimeSlice, 4, kTimeSliceIds),
    LIST_ELEMENT_INFO(kWebMIdTracks, 1, kTracksIds),
    LIST_ELEMENT_INFO(kWebMIdTrackEntry, 2, kTrackEntryIds),
    LIST_ELEMENT_INFO(kWebMIdTrackTranslate, 3, kTrackTranslateIds),
    LIST_ELEMENT_INFO(kWebMIdVideo, 3, kVideoIds),
    LIST_ELEMENT_INFO(kWebMIdAudio, 3, kAudioIds),
    LIST_ELEMENT_INFO(kWebMIdTrackOperation, 3, kTrackOperationIds),
    LIST_ELEMENT_INFO(kWebMIdTrackCombinePlanes, 4, kTrackCombinePlanesIds),
    LIST_ELEMENT_INFO(kWebMIdTrackPlane, 5, kTrackPlaneIds),
    LIST_ELEMENT_INFO(kWebMIdJoinBlocks, 4, kJoinBlocksIds),
    LIST_ELEMENT_INFO(kWebMIdContentEncodings, 3, kContentEncodingsIds),
    LIST_ELEMENT_INFO(kWebMIdContentEncoding, 4, kContentEncodingIds),
    LIST_ELEMENT_INFO(kWebMIdContentCompression, 5, kContentCompressionIds),
    LIST_ELEMENT_INFO(kWebMIdContentEncryption, 5, kContentEncryptionIds),
    LIST_ELEMENT_INFO(kWebMIdContentEncAESSettings,
                      6,
                      kContentEncAESSettingsIds),
    LIST_ELEMENT_INFO(kWebMIdCues, 1, kCuesIds),
    LIST_ELEMENT_INFO(kWebMIdCuePoint, 2, kCuePointIds),
    LIST_ELEMENT_INFO(kWebMIdCueTrackPositions, 3, kCueTrackPositionsIds),
    LIST_ELEMENT_INFO(kWebMIdCueReference, 4, kCueReferenceIds),
    LIST_ELEMENT_INFO(kWebMIdAttachments, 1, kAttachmentsIds),
    LIST_ELEMENT_INFO(kWebMIdAttachedFile, 2, kAttachedFileIds),
    LIST_ELEMENT_INFO(kWebMIdChapters, 1, kChaptersIds),
    LIST_ELEMENT_INFO(kWebMIdEditionEntry, 2, kEditionEntryIds),
    LIST_ELEMENT_INFO(kWebMIdChapterAtom, 3, kChapterAtomIds),
    LIST_ELEMENT_INFO(kWebMIdChapterTrack, 4, kChapterTrackIds),
    LIST_ELEMENT_INFO(kWebMIdChapterDisplay, 4, kChapterDisplayIds),
    LIST_ELEMENT_INFO(kWebMIdChapProcess, 4, kChapProcessIds),
    LIST_ELEMENT_INFO(kWebMIdChapProcessCommand, 5, kChapProcessCommandIds),
    LIST_ELEMENT_INFO(kWebMIdTags, 1, kTagsIds),
    LIST_ELEMENT_INFO(kWebMIdTag, 2, kTagIds),
    LIST_ELEMENT_INFO(kWebMIdTargets, 3, kTargetsIds),
    LIST_ELEMENT_INFO(kWebMIdSimpleTag, 3, kSimpleTagIds),
    LIST_ELEMENT_INFO(kWebMIdColour, 4, kColourIds),
    LIST_ELEMENT_INFO(kWebMIdColorVolumeMetadata, 5, kColorVolumeMetadataIds),
    LIST_ELEMENT_INFO(kWebMIdProjection, 4, kProjectionIds),
};

// Parses an element header id or size field. These fields are variable length
// encoded. The first byte indicates how many bytes the field occupies.
// |buf|  - The buffer to parse.
// |size| - The number of bytes in |buf|
// |max_bytes| - The maximum number of bytes the field can be. ID fields
//               set this to 4 & element size fields set this to 8. If the
//               first byte indicates a larger field size than this it is a
//               parser error.
// |mask_first_byte| - For element size fields the field length encoding bits
//                     need to be masked off. This parameter is true for
//                     element size fields and is false for ID field values.
//
// Returns: The number of bytes parsed on success. -1 on error.
static int ParseWebMElementHeaderField(const uint8_t* buf,
                                       int size,
                                       int max_bytes,
                                       bool mask_first_byte,
                                       int64_t* num) {
  DCHECK(buf);
  DCHECK(num);

  if (size < 0)
    return -1;

  if (size == 0)
    return 0;

  int mask = 0x80;
  uint8_t ch = buf[0];
  int extra_bytes = -1;
  bool all_ones = false;
  for (int i = 0; i < max_bytes; ++i) {
    if ((ch & mask) != 0) {
      mask = ~mask & 0xff;
      *num = mask_first_byte ? ch & mask : ch;
      all_ones = (ch & mask) == mask;
      extra_bytes = i;
      break;
    }
    mask = 0x80 | mask >> 1;
  }

  if (extra_bytes == -1)
    return -1;

  // Return 0 if we need more data.
  if ((1 + extra_bytes) > size)
    return 0;

  int bytes_used = 1;

  for (int i = 0; i < extra_bytes; ++i) {
    ch = buf[bytes_used++];
    all_ones &= (ch == 0xff);
    *num = (*num << 8) | ch;
  }

  if (all_ones)
    *num = std::numeric_limits<int64_t>::max();

  return bytes_used;
}

int WebMParseElementHeader(const uint8_t* buf,
                           int size,
                           int* id,
                           int64_t* element_size) {
  DCHECK(buf);
  DCHECK_GE(size, 0);
  DCHECK(id);
  DCHECK(element_size);

  if (size == 0)
    return 0;

  int64_t tmp = 0;
  int num_id_bytes = ParseWebMElementHeaderField(buf, size, 4, false, &tmp);

  if (num_id_bytes <= 0)
    return num_id_bytes;

  if (tmp == std::numeric_limits<int64_t>::max())
    tmp = kWebMReservedId;

  *id = static_cast<int>(tmp);

  int num_size_bytes = ParseWebMElementHeaderField(buf + num_id_bytes,
                                                   size - num_id_bytes,
                                                   8, true, &tmp);

  if (num_size_bytes <= 0)
    return num_size_bytes;

  if (tmp == std::numeric_limits<int64_t>::max())
    tmp = kWebMUnknownSize;

  *element_size = tmp;
  DVLOG(3) << "WebMParseElementHeader() : id " << std::hex << *id << std::dec
           << " size " << *element_size;
  return num_id_bytes + num_size_bytes;
}

// Finds ElementType for a specific ID.
static ElementType FindIdType(int id,
                              const ElementIdInfo* id_info,
                              int id_info_count) {

  // Check for global element IDs that can be anywhere.
  if (id == kWebMIdVoid || id == kWebMIdCRC32)
    return SKIP;

  for (int i = 0; i < id_info_count; ++i) {
    if (id == id_info[i].id_)
      return id_info[i].type_;
  }

  return UNKNOWN;
}

// Finds ListElementInfo for a specific ID.
static const ListElementInfo* FindListInfo(int id) {
  for (size_t i = 0; i < std::size(kListElementInfo); ++i) {
    if (id == kListElementInfo[i].id_)
      return &kListElementInfo[i];
  }

  return NULL;
}

static int FindListLevel(int id) {
  const ListElementInfo* list_info = FindListInfo(id);
  if (list_info)
    return list_info->level_;

  return -1;
}

static int ParseUInt(const uint8_t* buf,
                     int size,
                     int id,
                     WebMParserClient* client) {
  if ((size <= 0) || (size > 8))
    return -1;

  // Read in the big-endian integer.
  uint64_t value = 0;
  for (int i = 0; i < size; ++i)
    value = (value << 8) | buf[i];

  // We use int64_t in place of uint64_t everywhere for convenience.  See this
  // bug
  // for more details: http://crbug.com/366750#c3
  if (!base::IsValueInRangeForNumericType<int64_t>(value))
    return -1;

  if (!client->OnUInt(id, value))
    return -1;

  return size;
}

static int ParseFloat(const uint8_t* buf,
                      int size,
                      int id,
                      WebMParserClient* client) {
  if ((size != 4) && (size != 8))
    return -1;

  double value = -1;

  // Read the bytes from big-endian form into a native endian integer.
  int64_t tmp = 0;
  for (int i = 0; i < size; ++i)
    tmp = (tmp << 8) | buf[i];

  // Use a union to convert the integer bit pattern into a floating point
  // number.
  if (size == 4) {
    union {
      int32_t src;
      float dst;
    } tmp2;
    tmp2.src = static_cast<int32_t>(tmp);
    value = tmp2.dst;
  } else if (size == 8) {
    union {
      int64_t src;
      double dst;
    } tmp2;
    tmp2.src = tmp;
    value = tmp2.dst;
  } else {
    return -1;
  }

  if (!client->OnFloat(id, value))
    return -1;

  return size;
}

static int ParseBinary(const uint8_t* buf,
                       int size,
                       int id,
                       WebMParserClient* client) {
  return client->OnBinary(id, buf, size) ? size : -1;
}

static int ParseString(const uint8_t* buf,
                       int size,
                       int id,
                       WebMParserClient* client) {
  const uint8_t* end = static_cast<const uint8_t*>(memchr(buf, '\0', size));
  int length = (end != NULL) ? static_cast<int>(end - buf) : size;
  std::string str(reinterpret_cast<const char*>(buf), length);
  return client->OnString(id, str) ? size : -1;
}

static int ParseNonListElement(ElementType type,
                               int id,
                               int64_t element_size,
                               const uint8_t* buf,
                               int size,
                               WebMParserClient* client) {
  DCHECK_GE(size, element_size);

  int result = -1;
  switch(type) {
    case LIST:
      NOTIMPLEMENTED();
      result = -1;
      break;
    case UINT:
      result = ParseUInt(buf, element_size, id, client);
      break;
    case FLOAT:
      result = ParseFloat(buf, element_size, id, client);
      break;
    case BINARY:
      result = ParseBinary(buf, element_size, id, client);
      break;
    case STRING:
      result = ParseString(buf, element_size, id, client);
      break;
    case SKIP:
      result = element_size;
      break;
    default:
      DVLOG(1) << "Unhandled ID type " << type;
      return -1;
  };

  DCHECK_LE(result, size);
  return result;
}

WebMParserClient::WebMParserClient() = default;
WebMParserClient::~WebMParserClient() = default;

WebMParserClient* WebMParserClient::OnListStart(int id) {
  DVLOG(1) << "Unexpected list element start with ID " << std::hex << id;
  return NULL;
}

bool WebMParserClient::OnListEnd(int id) {
  DVLOG(1) << "Unexpected list element end with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnUInt(int id, int64_t val) {
  DVLOG(1) << "Unexpected unsigned integer element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnFloat(int id, double val) {
  DVLOG(1) << "Unexpected float element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnBinary(int id, const uint8_t* data, int size) {
  DVLOG(1) << "Unexpected binary element with ID " << std::hex << id;
  return false;
}

bool WebMParserClient::OnString(int id, const std::string& str) {
  DVLOG(1) << "Unexpected string element with ID " << std::hex << id;
  return false;
}

WebMListParser::WebMListParser(int id, WebMParserClient* client)
    : state_(NEED_LIST_HEADER),
      root_id_(id),
      root_level_(FindListLevel(id)),
      root_client_(client) {
  DCHECK_GE(root_level_, 0);
  DCHECK(client);
}

WebMListParser::~WebMListParser() = default;

void WebMListParser::Reset() {
  ChangeState(NEED_LIST_HEADER);
  list_state_stack_.clear();
}

int WebMListParser::Parse(const uint8_t* buf, int size) {
  DCHECK(buf);

  if (size < 0 || state_ == PARSE_ERROR || state_ == DONE_PARSING_LIST)
    return -1;

  if (size == 0)
    return 0;

  const uint8_t* cur = buf;
  int cur_size = size;
  int bytes_parsed = 0;

  while (cur_size > 0 && state_ != PARSE_ERROR && state_ != DONE_PARSING_LIST) {
    int element_id = 0;
    int64_t element_size = 0;
    int result = WebMParseElementHeader(cur, cur_size, &element_id,
                                        &element_size);

    if (result < 0)
      return result;

    if (result == 0)
      return bytes_parsed;

    switch(state_) {
      case NEED_LIST_HEADER: {
        if (element_id != root_id_) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        // Only allow Segment & Cluster to have an unknown size.
        if (element_size == kWebMUnknownSize &&
            (element_id != kWebMIdSegment) &&
            (element_id != kWebMIdCluster)) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        ChangeState(INSIDE_LIST);
        if (!OnListStart(root_id_, element_size))
          return -1;

        break;
      }

      case INSIDE_LIST: {
        int header_size = result;
        const uint8_t* element_data = cur + header_size;
        int element_data_size = cur_size - header_size;

        if (element_size < element_data_size)
          element_data_size = element_size;

        result = ParseListElement(header_size, element_id, element_size,
                                  element_data, element_data_size);

        DCHECK_LE(result, header_size + element_data_size);
        if (result < 0) {
          ChangeState(PARSE_ERROR);
          return -1;
        }

        if (result == 0)
          return bytes_parsed;

        break;
      }
      case DONE_PARSING_LIST:
      case PARSE_ERROR:
        // Shouldn't be able to get here.
        NOTIMPLEMENTED();
        break;
    }

    cur += result;
    cur_size -= result;
    bytes_parsed += result;
  }

  return (state_ == PARSE_ERROR) ? -1 : bytes_parsed;
}

bool WebMListParser::IsParsingComplete() const {
  return state_ == DONE_PARSING_LIST;
}

void WebMListParser::ChangeState(State new_state) {
  state_ = new_state;
}

int WebMListParser::ParseListElement(int header_size,
                                     int id,
                                     int64_t element_size,
                                     const uint8_t* data,
                                     int size) {
  DCHECK_GT(list_state_stack_.size(), 0u);

  ListState& list_state = list_state_stack_.back();
  DCHECK(list_state.element_info_);

  const ListElementInfo* element_info = list_state.element_info_;
  ElementType id_type =
      FindIdType(id, element_info->id_info_, element_info->id_info_count_);

  // Unexpected ID.
  if (id_type == UNKNOWN) {
    if (list_state.size_ != kWebMUnknownSize ||
        !IsSiblingOrAncestor(list_state.id_, id)) {
      DVLOG(1) << "No ElementType info for ID 0x" << std::hex << id;
      return -1;
    }

    // We've reached the end of a list of unknown size. Update the size now that
    // we know it and dispatch the end of list calls.
    list_state.size_ = list_state.bytes_parsed_;

    if (!OnListEnd())
      return -1;

    // Check to see if all open lists have ended.
    if (list_state_stack_.size() == 0)
      return 0;

    list_state = list_state_stack_.back();
  }

  // Make sure the whole element can fit inside the current list.
  int64_t total_element_size = header_size + element_size;
  if (list_state.size_ != kWebMUnknownSize &&
      list_state.size_ < list_state.bytes_parsed_ + total_element_size) {
    return -1;
  }

  if (id_type == LIST) {
    list_state.bytes_parsed_ += header_size;

    if (!OnListStart(id, element_size))
      return -1;
    return header_size;
  }

  // Make sure we have the entire element before trying to parse a non-list
  // element.
  if (size < element_size)
    return 0;

  int bytes_parsed = ParseNonListElement(id_type, id, element_size,
                                         data, size, list_state.client_);
  DCHECK_LE(bytes_parsed, size);

  // Return if an error occurred or we need more data.
  // Note: bytes_parsed is 0 for a successful parse of a size 0 element. We
  // need to check the element_size to disambiguate the "need more data" case
  // from a successful parse.
  if (bytes_parsed < 0 || (bytes_parsed == 0 && element_size != 0))
    return bytes_parsed;

  int result = header_size + bytes_parsed;
  list_state.bytes_parsed_ += result;

  // See if we have reached the end of the current list.
  if (list_state.bytes_parsed_ == list_state.size_) {
    if (!OnListEnd())
      return -1;
  }

  return result;
}

bool WebMListParser::OnListStart(int id, int64_t size) {
  const ListElementInfo* element_info = FindListInfo(id);
  if (!element_info)
    return false;

  int current_level = root_level_ + list_state_stack_.size() - 1;
  if (current_level + 1 != element_info->level_)
    return false;

  WebMParserClient* current_list_client = NULL;
  if (!list_state_stack_.empty()) {
    // Make sure the new list doesn't go past the end of the current list.
    ListState current_list_state = list_state_stack_.back();
    if (current_list_state.size_ != kWebMUnknownSize &&
        current_list_state.size_ < current_list_state.bytes_parsed_ + size)
      return false;
    current_list_client = current_list_state.client_;
  } else {
    current_list_client = root_client_;
  }

  WebMParserClient* new_list_client = current_list_client->OnListStart(id);
  if (!new_list_client)
    return false;

  ListState new_list_state = { id, size, 0, element_info, new_list_client };
  list_state_stack_.push_back(new_list_state);

  if (size == 0)
    return OnListEnd();

  return true;
}

bool WebMListParser::OnListEnd() {
  int lists_ended = 0;
  for (; !list_state_stack_.empty(); ++lists_ended) {
    const ListState& list_state = list_state_stack_.back();
    int64_t bytes_parsed = list_state.bytes_parsed_;
    int id = list_state.id_;

    if (bytes_parsed != list_state.size_)
      break;

    list_state_stack_.pop_back();

    WebMParserClient* client = NULL;
    if (!list_state_stack_.empty()) {
      // Update the bytes_parsed_ for the parent element.
      list_state_stack_.back().bytes_parsed_ += bytes_parsed;
      client = list_state_stack_.back().client_;
    } else {
      client = root_client_;
    }

    if (!client->OnListEnd(id))
      return false;
  }

  DCHECK_GE(lists_ended, 1);

  if (list_state_stack_.empty())
    ChangeState(DONE_PARSING_LIST);

  return true;
}

bool WebMListParser::IsSiblingOrAncestor(int id_a, int id_b) const {
  if (id_a == kWebMIdCluster) {
    // kWebMIdCluster siblings.
    for (size_t i = 0; i < std::size(kSegmentIds); i++) {
      if (kSegmentIds[i].id_ == id_b)
        return true;
    }
  } else if (id_a != kWebMIdSegment) {
    return false;
  }

  // kWebMIdSegment sibling or ancestor, respectively; kWebMIdCluster ancestors.
  return ((id_b == kWebMIdSegment) || (id_b == kWebMIdEBMLHeader));
}

}  // namespace media
