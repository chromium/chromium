// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_config_util.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/logging.h"
#include "media/base/mac/color_space_util_mac.h"
#include "ui/gfx/hdr_metadata_mac.h"

namespace {

// https://developer.apple.com/documentation/avfoundation/avassettrack/1386694-formatdescriptions?language=objc
NSString* CMVideoCodecTypeToString(CMVideoCodecType code) {
  NSString* result = [NSString
      stringWithFormat:@"%c%c%c%c", (code >> 24) & 0xff, (code >> 16) & 0xff,
                       (code >> 8) & 0xff, code & 0xff];
  NSCharacterSet* characterSet = [NSCharacterSet whitespaceCharacterSet];
  return [result stringByTrimmingCharactersInSet:characterSet];
}

// Helper functions to convert from CFStringRef kCM* keys to NSString.
void SetDictionaryValue(NSMutableDictionary<NSString*, id>* dictionary,
                        CFStringRef key,
                        id value) {
  if (value) {
    dictionary[base::apple::CFToNSPtrCast(key)] = value;
  }
}

void SetDictionaryValue(NSMutableDictionary<NSString*, id>* dictionary,
                        CFStringRef key,
                        CFStringRef value) {
  SetDictionaryValue(dictionary, key, base::apple::CFToNSPtrCast(value));
}

CFStringRef GetPrimaries(media::VideoColorSpace::PrimaryID primary_id) {
  switch (primary_id) {
    case media::VideoColorSpace::PrimaryID::BT709:
    case media::VideoColorSpace::PrimaryID::UNSPECIFIED:  // Assume BT.709.
    case media::VideoColorSpace::PrimaryID::INVALID:      // Assume BT.709.
      return kCMFormatDescriptionColorPrimaries_ITU_R_709_2;

    case media::VideoColorSpace::PrimaryID::BT2020:
      return kCMFormatDescriptionColorPrimaries_ITU_R_2020;

    case media::VideoColorSpace::PrimaryID::SMPTE170M:
    case media::VideoColorSpace::PrimaryID::SMPTE240M:
      return kCMFormatDescriptionColorPrimaries_SMPTE_C;

    case media::VideoColorSpace::PrimaryID::BT470BG:
    case media::VideoColorSpace::PrimaryID::EBU_3213_E:
      // Based on ITU H.273 8.1, there is a slight discrepancy between BT470 BG
      // and EBU 3213 E, but a careful reading of E.B.U Tech 3213-E (1975) shows
      // the primaries are identical.
      return kCMFormatDescriptionColorPrimaries_EBU_3213;

    case media::VideoColorSpace::PrimaryID::SMPTEST431_2:
      return kCMFormatDescriptionColorPrimaries_DCI_P3;

    case media::VideoColorSpace::PrimaryID::SMPTEST432_1:
      return kCMFormatDescriptionColorPrimaries_P3_D65;

    default:
      DLOG(ERROR) << "Unsupported primary id: " << static_cast<int>(primary_id);
      return nil;
  }
}

CFStringRef GetTransferFunction(
    media::VideoColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case media::VideoColorSpace::TransferID::LINEAR:
      return kCMFormatDescriptionTransferFunction_Linear;

    case media::VideoColorSpace::TransferID::GAMMA22:
    case media::VideoColorSpace::TransferID::GAMMA28:
      return kCMFormatDescriptionTransferFunction_UseGamma;

    case media::VideoColorSpace::TransferID::IEC61966_2_1:
      return kCVImageBufferTransferFunction_sRGB;

    case media::VideoColorSpace::TransferID::SMPTE170M:
    case media::VideoColorSpace::TransferID::BT709:
    case media::VideoColorSpace::TransferID::UNSPECIFIED:  // Assume BT.709.
    case media::VideoColorSpace::TransferID::INVALID:      // Assume BT.709.
      return kCMFormatDescriptionTransferFunction_ITU_R_709_2;

    case media::VideoColorSpace::TransferID::BT2020_10:
    case media::VideoColorSpace::TransferID::BT2020_12:
      return kCMFormatDescriptionTransferFunction_ITU_R_2020;

    case media::VideoColorSpace::TransferID::SMPTEST2084:
      return kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;

    case media::VideoColorSpace::TransferID::ARIB_STD_B67:
      return kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;

    case media::VideoColorSpace::TransferID::SMPTE240M:
      return kCMFormatDescriptionTransferFunction_SMPTE_240M_1995;

    case media::VideoColorSpace::TransferID::SMPTEST428_1:
      return kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;

    default:
      DLOG(ERROR) << "Unsupported transfer function: "
                  << static_cast<int>(transfer_id);
      return nil;
  }
}

CFStringRef GetMatrix(media::VideoColorSpace::MatrixID matrix_id) {
  switch (matrix_id) {
    case media::VideoColorSpace::MatrixID::BT709:
    case media::VideoColorSpace::MatrixID::UNSPECIFIED:  // Assume BT.709.
    case media::VideoColorSpace::MatrixID::INVALID:      // Assume BT.709.
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2;

    case media::VideoColorSpace::MatrixID::BT2020_NCL:
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;

    case media::VideoColorSpace::MatrixID::FCC:
    case media::VideoColorSpace::MatrixID::SMPTE170M:
    case media::VideoColorSpace::MatrixID::BT470BG:
      // The FCC-based coefficients don't exactly match BT.601, but they're
      // close enough.
      return kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4;

    case media::VideoColorSpace::MatrixID::SMPTE240M:
      return kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995;

    default:
      DLOG(ERROR) << "Unsupported matrix id: " << static_cast<int>(matrix_id);
      return nil;
  }
}

void SetContentLightLevelInfo(
    NSMutableDictionary<NSString*, id>* extensions,
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  SetDictionaryValue(
      extensions, kCMFormatDescriptionExtension_ContentLightLevelInfo,
      base::apple::CFToNSPtrCast(
          gfx::GenerateContentLightLevelInfo(hdr_metadata).get()));
}

void SetColorVolumeMetadata(
    NSMutableDictionary<NSString*, id>* extensions,
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  SetDictionaryValue(
      extensions, kCMFormatDescriptionExtension_MasteringDisplayColorVolume,
      base::apple::CFToNSPtrCast(
          gfx::GenerateMasteringDisplayColorVolume(hdr_metadata).get()));
}

void SetVp9CodecConfigurationBox(NSMutableDictionary<NSString*, id>* extensions,
                                 media::VideoCodecProfile codec_profile,
                                 const media::VideoColorSpace& color_space) {
  // Synthesize a 'vpcC' box. See
  // https://www.webmproject.org/vp9/mp4/#vp-codec-configuration-box.
  uint8_t version = 1;
  uint8_t profile = 0;
  uint8_t level = 51;
  uint8_t bit_depth = 8;
  uint8_t chroma_subsampling = 1;  // 4:2:0 colocated with luma (0, 0).
  uint8_t primaries = 1;           // BT.709.
  uint8_t transfer = 1;            // BT.709.
  uint8_t matrix = 1;              // BT.709.

  if (color_space.IsSpecified()) {
    primaries = static_cast<uint8_t>(color_space.primaries);
    transfer = static_cast<uint8_t>(color_space.transfer);
    matrix = static_cast<uint8_t>(color_space.matrix);
  }

  if (codec_profile == media::VP9PROFILE_PROFILE2) {
    profile = 2;
    bit_depth = 10;
  }

  uint8_t vpcc[12] = {0};
  vpcc[0] = version;
  vpcc[4] = profile;
  vpcc[5] = level;
  vpcc[6] |= bit_depth << 4;
  vpcc[6] |= chroma_subsampling << 1;
  vpcc[7] = primaries;
  vpcc[8] = transfer;
  vpcc[9] = matrix;
  SetDictionaryValue(
      extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      @{
        @"vpcC" : [NSData dataWithBytes:&vpcc length:sizeof(vpcc)],
      });
  SetDictionaryValue(extensions, CFSTR("BitsPerComponent"), @(bit_depth));
}

void SetAv1CodecConfigurationBox(NSMutableDictionary<NSString*, id>* extensions,
                                 int bit_depth,
                                 base::span<const uint8_t> av1c) {
  SetDictionaryValue(
      extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      @{
        @"av1C" : [NSData dataWithBytes:av1c.data() length:av1c.size()],
      });
  SetDictionaryValue(extensions, CFSTR("BitsPerComponent"), @(bit_depth));
}

}  // namespace

namespace media {

base::apple::ScopedCFTypeRef<CFDictionaryRef> CreateFormatExtensions(
    CMVideoCodecType codec_type,
    VideoCodecProfile profile,
    int bit_depth,
    const VideoColorSpace& color_space,
    std::optional<gfx::HDRMetadata> hdr_metadata,
    std::optional<base::span<const uint8_t>> csd_box) {
  NSMutableDictionary* extensions = [[NSMutableDictionary alloc] init];

  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_FormatName,
                     CMVideoCodecTypeToString(codec_type));

  // YCbCr without alpha uses 24. See
  // http://developer.apple.com/qa/qa2001/qa1183.html
  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_Depth, @24);

  // Set primaries.
  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_ColorPrimaries,
                     GetPrimaries(color_space.primaries));

  // Set transfer function.
  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_TransferFunction,
                     GetTransferFunction(color_space.transfer));
  if (color_space.transfer == VideoColorSpace::TransferID::GAMMA22) {
    SetDictionaryValue(extensions, kCMFormatDescriptionExtension_GammaLevel,
                       @2.2);
  } else if (color_space.transfer == VideoColorSpace::TransferID::GAMMA28) {
    SetDictionaryValue(extensions, kCMFormatDescriptionExtension_GammaLevel,
                       @2.8);
  }

  // Set matrix.
  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_YCbCrMatrix,
                     GetMatrix(color_space.matrix));

  // Set full range flag.
  SetDictionaryValue(extensions, kCMFormatDescriptionExtension_FullRangeVideo,
                     @(color_space.range == gfx::ColorSpace::RangeID::FULL));

  // Set metadata for PQ signals.
  if (color_space.transfer == VideoColorSpace::TransferID::SMPTEST2084) {
    SetContentLightLevelInfo(extensions, hdr_metadata);
    SetColorVolumeMetadata(extensions, hdr_metadata);
  }

  if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    SetVp9CodecConfigurationBox(extensions, profile, color_space);
  }

  if (profile >= AV1PROFILE_MIN && profile <= AV1PROFILE_MAX) {
    DCHECK(csd_box);
    SetAv1CodecConfigurationBox(extensions, bit_depth, *csd_box);
  }

  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      base::apple::NSToCFOwnershipCast(extensions));
}

}  // namespace media
