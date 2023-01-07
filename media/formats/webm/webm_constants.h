// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CONSTANTS_H_
#define MEDIA_FORMATS_WEBM_WEBM_CONSTANTS_H_

#include <stdint.h>

#include "media/base/media_export.h"

namespace media {

// WebM element IDs.
// This is a subset of the IDs in the Matroska spec.
// http://www.matroska.org/technical/specs/index.html
const int kWebMIdAESSettingsCipherMode = 0x47E8;
const int kWebMIdAlphaMode = 0x53C0;
const int kWebMIdAspectRatioType = 0x54B3;
const int kWebMIdAttachedFile = 0x61A7;
const int kWebMIdAttachmentLink = 0x7446;
const int kWebMIdAttachments = 0x1941A469;
const int kWebMIdAudio = 0xE1;
const int kWebMIdBitDepth = 0x6264;
const int kWebMIdBitsPerChannel = 0x55B2;
const int kWebMIdBlock = 0xA1;
const int kWebMIdBlockAddID = 0xEE;
const int kWebMIdBlockAdditional = 0xA5;
const int kWebMIdBlockAdditions = 0x75A1;
const int kWebMIdBlockDuration = 0x9B;
const int kWebMIdBlockGroup = 0xA0;
const int kWebMIdBlockMore = 0xA6;
const int kWebMIdCbSubsamplingHorz = 0x55B5;
const int kWebMIdCbSubsamplingVert = 0x55B6;
const int kWebMIdChannels = 0x9F;
const int kWebMIdChapCountry = 0x437E;
const int kWebMIdChapLanguage = 0x437C;
const int kWebMIdChapProcess = 0x6944;
const int kWebMIdChapProcessCodecID = 0x6955;
const int kWebMIdChapProcessCommand = 0x6911;
const int kWebMIdChapProcessData = 0x6933;
const int kWebMIdChapProcessPrivate = 0x450D;
const int kWebMIdChapProcessTime = 0x6922;
const int kWebMIdChapString = 0x85;
const int kWebMIdChapterAtom = 0xB6;
const int kWebMIdChapterDisplay = 0x80;
const int kWebMIdChapterFlagEnabled = 0x4598;
const int kWebMIdChapterFlagHidden = 0x98;
const int kWebMIdChapterPhysicalEquiv = 0x63C3;
const int kWebMIdChapters = 0x1043A770;
const int kWebMIdChapterSegmentEditionUID = 0x6EBC;
const int kWebMIdChapterSegmentUID = 0x6E67;
const int kWebMIdChapterTimeEnd = 0x92;
const int kWebMIdChapterTimeStart = 0x91;
const int kWebMIdChapterTrack = 0x8F;
const int kWebMIdChapterTrackNumber = 0x89;
const int kWebMIdChapterTranslate = 0x6924;
const int kWebMIdChapterTranslateCodec = 0x69BF;
const int kWebMIdChapterTranslateEditionUID = 0x69FC;
const int kWebMIdChapterTranslateID = 0x69A5;
const int kWebMIdChapterUID = 0x73C4;
const int kWebMIdChromaSitingHorz = 0x55B7;
const int kWebMIdChromaSitingVert = 0x55B8;
const int kWebMIdChromaSubsamplingHorz = 0x55B3;
const int kWebMIdChromaSubsamplingVert = 0x55B4;
const int kWebMIdCluster = 0x1F43B675;
const int kWebMIdCodecDecodeAll = 0xAA;
const int kWebMIdCodecDelay = 0x56AA;
const int kWebMIdCodecID = 0x86;
const int kWebMIdCodecName = 0x258688;
const int kWebMIdCodecPrivate = 0x63A2;
const int kWebMIdCodecState = 0xA4;
const int kWebMIdColorSpace = 0x2EB524;
const int kWebMIdColour = 0x55B0;
const int kWebMIdContentCompAlgo = 0x4254;
const int kWebMIdContentCompression = 0x5034;
const int kWebMIdContentCompSettings = 0x4255;
const int kWebMIdContentEncAESSettings = 0x47E7;
const int kWebMIdContentEncAlgo = 0x47E1;
const int kWebMIdContentEncKeyID = 0x47E2;
const int kWebMIdContentEncoding = 0x6240;
const int kWebMIdContentEncodingOrder = 0x5031;
const int kWebMIdContentEncodings = 0x6D80;
const int kWebMIdContentEncodingScope = 0x5032;
const int kWebMIdContentEncodingType = 0x5033;
const int kWebMIdContentEncryption = 0x5035;
const int kWebMIdContentSigAlgo = 0x47E5;
const int kWebMIdContentSigHashAlgo = 0x47E6;
const int kWebMIdContentSigKeyID = 0x47E4;
const int kWebMIdContentSignature = 0x47E3;
const int kWebMIdCRC32 = 0xBF;
const int kWebMIdCueBlockNumber = 0x5378;
const int kWebMIdCueClusterPosition = 0xF1;
const int kWebMIdCueCodecState = 0xEA;
const int kWebMIdCuePoint = 0xBB;
const int kWebMIdCueReference = 0xDB;
const int kWebMIdCueRefTime = 0x96;
const int kWebMIdCues = 0x1C53BB6B;
const int kWebMIdCueTime = 0xB3;
const int kWebMIdCueTrack = 0xF7;
const int kWebMIdCueTrackPositions = 0xB7;
const int kWebMIdDateUTC = 0x4461;
const int kWebMIdDefaultDuration = 0x23E383;
const int kWebMIdDiscardPadding = 0x75A2;
const int kWebMIdDisplayHeight = 0x54BA;
const int kWebMIdDisplayUnit = 0x54B2;
const int kWebMIdDisplayWidth = 0x54B0;
const int kWebMIdDocType = 0x4282;
const int kWebMIdDocTypeReadVersion = 0x4285;
const int kWebMIdDocTypeVersion = 0x4287;
const int kWebMIdDuration = 0x4489;
const int kWebMIdEBMLHeader = 0x1A45DFA3;
const int kWebMIdEBMLMaxIDLength = 0x42F2;
const int kWebMIdEBMLMaxSizeLength = 0x42F3;
const int kWebMIdEBMLReadVersion = 0x42F7;
const int kWebMIdEBMLVersion = 0x4286;
const int kWebMIdEditionEntry = 0x45B9;
const int kWebMIdEditionFlagDefault = 0x45DB;
const int kWebMIdEditionFlagHidden = 0x45BD;
const int kWebMIdEditionFlagOrdered = 0x45DD;
const int kWebMIdEditionUID = 0x45BC;
const int kWebMIdFileData = 0x465C;
const int kWebMIdFileDescription = 0x467E;
const int kWebMIdFileMimeType = 0x4660;
const int kWebMIdFileName = 0x466E;
const int kWebMIdFileUID = 0x46AE;
const int kWebMIdFlagDefault = 0x88;
const int kWebMIdFlagEnabled = 0xB9;
const int kWebMIdFlagForced = 0x55AA;
const int kWebMIdFlagInterlaced = 0x9A;
const int kWebMIdFlagLacing = 0x9C;
const int kWebMIdFrameRate = 0x2383E3;
const int kWebMIdInfo = 0x1549A966;
const int kWebMIdJoinBlocks = 0xE9;
const int kWebMIdLaceNumber = 0xCC;
const int kWebMIdLanguage = 0x22B59C;
const int kWebMIdLuminanceMax = 0x55D9;
const int kWebMIdLuminanceMin = 0x55DA;
const int kWebMIdColorVolumeMetadata = 0x55D0;
const int kWebMIdMatrixCoefficients = 0x55B1;
const int kWebMIdMaxBlockAdditionId = 0x55EE;
const int kWebMIdMaxCache = 0x6DF8;
const int kWebMIdMaxCLL = 0x55BC;
const int kWebMIdMaxFALL = 0x55BD;
const int kWebMIdMinCache = 0x6DE7;
const int kWebMIdMuxingApp = 0x4D80;
const int kWebMIdName = 0x536E;
const int kWebMIdNextFilename = 0x3E83BB;
const int kWebMIdNextUID = 0x3EB923;
const int kWebMIdOutputSamplingFrequency = 0x78B5;
const int kWebMIdPixelCropBottom = 0x54AA;
const int kWebMIdPixelCropLeft = 0x54CC;
const int kWebMIdPixelCropRight = 0x54DD;
const int kWebMIdPixelCropTop = 0x54BB;
const int kWebMIdPixelHeight = 0xBA;
const int kWebMIdPixelWidth = 0xB0;
const int kWebMIdPosition = 0xA7;
const int kWebMIdPrevFilename = 0x3C83AB;
const int kWebMIdPrevSize = 0xAB;
const int kWebMIdPrevUID = 0x3CB923;
const int kWebMIdPrimaries = 0x55BB;
const int kWebMIdPrimaryBChromaticityX = 0x55D5;
const int kWebMIdPrimaryBChromaticityY = 0x55D6;
const int kWebMIdPrimaryGChromaticityX = 0x55D3;
const int kWebMIdPrimaryGChromaticityY = 0x55D4;
const int kWebMIdPrimaryRChromaticityX = 0x55D1;
const int kWebMIdPrimaryRChromaticityY = 0x55D2;
const int kWebMIdProjection = 0x7670;
const int kWebMIdProjectionPosePitch = 0x7674;
const int kWebMIdProjectionPoseRoll = 0x7675;
const int kWebMIdProjectionPoseYaw = 0x7673;
const int kWebMIdProjectionPrivate = 0x7672;
const int kWebMIdProjectionType = 0x7671;
const int kWebMIdRange = 0x55B9;
const int kWebMIdReferenceBlock = 0xFB;
const int kWebMIdReferencePriority = 0xFA;
const int kWebMIdSamplingFrequency = 0xB5;
const int kWebMIdSeek = 0x4DBB;
const int kWebMIdSeekHead = 0x114D9B74;
const int kWebMIdSeekID = 0x53AB;
const int kWebMIdSeekPosition = 0x53AC;
const int kWebMIdSeekPreRoll = 0x56BB;
const int kWebMIdSegment = 0x18538067;
const int kWebMIdSegmentFamily = 0x4444;
const int kWebMIdSegmentFilename = 0x7384;
const int kWebMIdSegmentUID = 0x73A4;
const int kWebMIdSilentTrackNumber = 0x58D7;
const int kWebMIdSilentTracks = 0x5854;
const int kWebMIdSimpleBlock = 0xA3;
const int kWebMIdSimpleTag = 0x67C8;
const int kWebMIdSlices = 0x8E;
const int kWebMIdStereoMode = 0x53B8;
const int kWebMIdTag = 0x7373;
const int kWebMIdTagAttachmentUID = 0x63C6;
const int kWebMIdTagBinary = 0x4485;
const int kWebMIdTagChapterUID = 0x63C4;
const int kWebMIdTagDefault = 0x4484;
const int kWebMIdTagEditionUID = 0x63C9;
const int kWebMIdTagLanguage = 0x447A;
const int kWebMIdTagName = 0x45A3;
const int kWebMIdTags = 0x1254C367;
const int kWebMIdTagString = 0x4487;
const int kWebMIdTagTrackUID = 0x63C5;
const int kWebMIdTargets = 0x63C0;
const int kWebMIdTargetType = 0x63CA;
const int kWebMIdTargetTypeValue = 0x68CA;
const int kWebMIdTimecode = 0xE7;
const int kWebMIdTimecodeScale = 0x2AD7B1;
const int kWebMIdTimeSlice = 0xE8;
const int kWebMIdTitle = 0x7BA9;
const int kWebMIdTrackCombinePlanes = 0xE3;
const int kWebMIdTrackEntry = 0xAE;
const int kWebMIdTrackJoinUID = 0xED;
const int kWebMIdTrackNumber = 0xD7;
const int kWebMIdTrackOperation = 0xE2;
const int kWebMIdTrackOverlay = 0x6FAB;
const int kWebMIdTrackPlane = 0xE4;
const int kWebMIdTrackPlaneType = 0xE6;
const int kWebMIdTrackPlaneUID = 0xE5;
const int kWebMIdTracks = 0x1654AE6B;
const int kWebMIdTrackTimecodeScale = 0x23314F;
const int kWebMIdTrackTranslate = 0x6624;
const int kWebMIdTrackTranslateCodec = 0x66BF;
const int kWebMIdTrackTranslateEditionUID = 0x66FC;
const int kWebMIdTrackTranslateTrackID = 0x66A5;
const int kWebMIdTrackType = 0x83;
const int kWebMIdTrackUID = 0x73C5;
const int kWebMIdTransferCharacteristics = 0x55BA;
const int kWebMIdVideo = 0xE0;
const int kWebMIdVoid = 0xEC;
const int kWebMIdWhitePointChromaticityX = 0x55D7;
const int kWebMIdWhitePointChromaticityY = 0x55D8;
const int kWebMIdWritingApp = 0x5741;

const int64_t kWebMReservedId = 0x1FFFFFFF;
const int64_t kWebMUnknownSize = 0x00FFFFFFFFFFFFFFLL;

const uint8_t kWebMFlagKeyframe = 0x80;

// Current encrypted WebM request for comments specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
const uint8_t kWebMFlagEncryptedFrame = 0x1;
const uint8_t kWebMFlagEncryptedFramePartitioned = 0x2;
const int kWebMIvSize = 8;
const int kWebMSignalByteSize = 1;
const int kWebMEncryptedFrameNumPartitionsSize = 1;
const int kWebMEncryptedFramePartitionOffsetSize = 4;

// Current specification for WebVTT embedded in WebM
// http://wiki.webmproject.org/webm-metadata/temporal-metadata/webvtt-in-webm

const int kWebMTrackTypeVideo = 1;
const int kWebMTrackTypeAudio = 2;
const int kWebMTrackTypeSubtitlesOrCaptions = 0x11;
const int kWebMTrackTypeDescriptionsOrMetadata = 0x21;

MEDIA_EXPORT extern const char kWebMCodecSubtitles[];
MEDIA_EXPORT extern const char kWebMCodecCaptions[];
MEDIA_EXPORT extern const char kWebMCodecDescriptions[];
MEDIA_EXPORT extern const char kWebMCodecMetadata[];

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CONSTANTS_H_
