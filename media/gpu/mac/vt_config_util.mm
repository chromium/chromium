// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vt_config_util.h"

#import <Foundation/Foundation.h>

#include <simd/simd.h>

#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"

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
  if (value)
    dictionary[base::mac::CFToNSCast(key)] = value;
}

void SetDictionaryValue(NSMutableDictionary<NSString*, id>* dictionary,
                        CFStringRef key,
                        CFStringRef value) {
  SetDictionaryValue(dictionary, key, base::mac::CFToNSCast(value));
}

CFStringRef GetPrimaries(media::VideoColorSpace::PrimaryID primary_id) {
  switch (primary_id) {
    case media::VideoColorSpace::PrimaryID::BT709:
    case media::VideoColorSpace::PrimaryID::UNSPECIFIED:  // Assume BT.709.
    case media::VideoColorSpace::PrimaryID::INVALID:      // Assume BT.709.
      return kCMFormatDescriptionColorPrimaries_ITU_R_709_2;

    case media::VideoColorSpace::PrimaryID::BT2020:
      if (@available(macos 10.11, *))
        return kCMFormatDescriptionColorPrimaries_ITU_R_2020;
      DLOG(WARNING) << "kCMFormatDescriptionColorPrimaries_ITU_R_2020 "
                       "unsupported prior to 10.11";
      return nil;

    case media::VideoColorSpace::PrimaryID::SMPTE170M:
    case media::VideoColorSpace::PrimaryID::SMPTE240M:
      return kCMFormatDescriptionColorPrimaries_SMPTE_C;

    case media::VideoColorSpace::PrimaryID::BT470BG:
      return kCMFormatDescriptionColorPrimaries_EBU_3213;

    case media::VideoColorSpace::PrimaryID::SMPTEST431_2:
      if (@available(macos 10.11, *))
        return kCMFormatDescriptionColorPrimaries_DCI_P3;
      DLOG(WARNING) << "kCMFormatDescriptionColorPrimaries_DCI_P3 unsupported "
                       "prior to 10.11";
      return nil;

    case media::VideoColorSpace::PrimaryID::SMPTEST432_1:
      if (@available(macos 10.11, *))
        return kCMFormatDescriptionColorPrimaries_P3_D65;
      DLOG(WARNING) << "kCMFormatDescriptionColorPrimaries_P3_D65 unsupported "
                       "prior to 10.11";
      return nil;

    default:
      DLOG(ERROR) << "Unsupported primary id: " << static_cast<int>(primary_id);
      return nil;
  }
}

CFStringRef GetTransferFunction(
    media::VideoColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case media::VideoColorSpace::TransferID::LINEAR:
      if (@available(macos 10.14, *))
        return kCMFormatDescriptionTransferFunction_Linear;
      DLOG(WARNING) << "kCMFormatDescriptionTransferFunction_Linear "
                       "unsupported prior to 10.14";
      return nil;

    case media::VideoColorSpace::TransferID::GAMMA22:
    case media::VideoColorSpace::TransferID::GAMMA28:
      return kCMFormatDescriptionTransferFunction_UseGamma;

    case media::VideoColorSpace::TransferID::IEC61966_2_1:
      if (@available(macos 10.13, *))
        return kCVImageBufferTransferFunction_sRGB;
      DLOG(WARNING)
          << "kCVImageBufferTransferFunction_sRGB unsupported prior to 10.13";
      return nil;

    case media::VideoColorSpace::TransferID::SMPTE170M:
    case media::VideoColorSpace::TransferID::BT709:
    case media::VideoColorSpace::TransferID::UNSPECIFIED:  // Assume BT.709.
    case media::VideoColorSpace::TransferID::INVALID:      // Assume BT.709.
      return kCMFormatDescriptionTransferFunction_ITU_R_709_2;

    case media::VideoColorSpace::TransferID::BT2020_10:
    case media::VideoColorSpace::TransferID::BT2020_12:
      if (@available(macos 10.11, *))
        return kCMFormatDescriptionTransferFunction_ITU_R_2020;
      DLOG(WARNING) << "kCMFormatDescriptionTransferFunction_ITU_R_2020 "
                       "unsupported prior to 10.11";
      return nil;

    case media::VideoColorSpace::TransferID::SMPTEST2084:
      if (@available(macos 10.13, *))
        return kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ;
      DLOG(WARNING) << "kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ "
                       "unsupported prior to 10.13";
      return nil;

    case media::VideoColorSpace::TransferID::ARIB_STD_B67:
      if (@available(macos 10.13, *))
        return kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG;
      DLOG(WARNING) << "kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG "
                       "unsupported prior to 10.13";
      return nil;

    case media::VideoColorSpace::TransferID::SMPTE240M:
      return kCMFormatDescriptionTransferFunction_SMPTE_240M_1995;

    case media::VideoColorSpace::TransferID::SMPTEST428_1:
      if (@available(macos 10.12, *))
        return kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1;
      DLOG(WARNING) << "kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1 "
                       "unsupported prior to 10.12";
      return nil;

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
      if (@available(macos 10.11, *))
        return kCMFormatDescriptionYCbCrMatrix_ITU_R_2020;
      DLOG(WARNING) << "kCVImageBufferYCbCrMatrix_ITU_R_2020 "
                       "unsupported prior to 10.11";
      return nil;

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

void SetContentLightLevelInfo(const gl::HDRMetadata& hdr_metadata,
                              NSMutableDictionary<NSString*, id>* extensions) {
  if (@available(macos 10.13, *)) {
    // This is a SMPTEST2086 Content Light Level Information box.
    struct ContentLightLevelInfoSEI {
      uint16_t max_content_light_level;
      uint16_t max_frame_average_light_level;
    } __attribute__((packed, aligned(2)));
    static_assert(sizeof(ContentLightLevelInfoSEI) == 4, "Must be 4 bytes");

    // Values are stored in big-endian...
    ContentLightLevelInfoSEI sei;
    sei.max_content_light_level =
        __builtin_bswap16(hdr_metadata.max_content_light_level);
    sei.max_frame_average_light_level =
        __builtin_bswap16(hdr_metadata.max_frame_average_light_level);

    NSData* nsdata_sei = [NSData dataWithBytes:&sei length:4];
    SetDictionaryValue(extensions,
                       kCMFormatDescriptionExtension_ContentLightLevelInfo,
                       nsdata_sei);
  } else {
    DLOG(WARNING) << "kCMFormatDescriptionExtension_ContentLightLevelInfo "
                     "unsupported prior to 10.13";
  }
}

void SetMasteringMetadata(const gl::HDRMetadata& hdr_metadata,
                          NSMutableDictionary<NSString*, id>* extensions) {
  if (@available(macos 10.13, *)) {
    // This is a SMPTEST2086 Mastering Display Color Volume box.
    struct MasteringDisplayColorVolumeSEI {
      vector_ushort2 primaries[3];  // GBR
      vector_ushort2 white_point;
      uint32_t luminance_max;
      uint32_t luminance_min;
    } __attribute__((packed, aligned(4)));
    static_assert(sizeof(MasteringDisplayColorVolumeSEI) == 24,
                  "Must be 24 bytes");

    // Make a copy which we can manipulate.
    auto md = hdr_metadata.mastering_metadata;

    constexpr float kColorCoordinateUpperBound = 50000.0f;
    md.primary_r.Scale(kColorCoordinateUpperBound);
    md.primary_g.Scale(kColorCoordinateUpperBound);
    md.primary_b.Scale(kColorCoordinateUpperBound);
    md.white_point.Scale(kColorCoordinateUpperBound);

    constexpr float kUnitOfMasteringLuminance = 10000.0f;
    md.luminance_max *= kUnitOfMasteringLuminance;
    md.luminance_min *= kUnitOfMasteringLuminance;

    // Values are stored in big-endian...
    MasteringDisplayColorVolumeSEI sei;
    sei.primaries[0].x = __builtin_bswap16(md.primary_g.x() + 0.5f);
    sei.primaries[0].y = __builtin_bswap16(md.primary_g.y() + 0.5f);
    sei.primaries[1].x = __builtin_bswap16(md.primary_b.x() + 0.5f);
    sei.primaries[1].y = __builtin_bswap16(md.primary_b.y() + 0.5f);
    sei.primaries[2].x = __builtin_bswap16(md.primary_r.x() + 0.5f);
    sei.primaries[2].y = __builtin_bswap16(md.primary_r.y() + 0.5f);
    sei.white_point.x = __builtin_bswap16(md.white_point.x() + 0.5f);
    sei.white_point.y = __builtin_bswap16(md.white_point.y() + 0.5f);
    sei.luminance_max = __builtin_bswap32(md.luminance_max + 0.5f);
    sei.luminance_min = __builtin_bswap32(md.luminance_min + 0.5f);

    NSData* nsdata_sei = [NSData dataWithBytes:&sei length:24];
    SetDictionaryValue(
        extensions, kCMFormatDescriptionExtension_MasteringDisplayColorVolume,
        nsdata_sei);
  } else {
    DLOG(WARNING) << "kCMFormatDescriptionExtension_"
                     "MasteringDisplayColorVolume unsupported prior to 10.13";
  }
}

// Read the value for the key in |key| to CFString and convert it to IdType.
// Use the list of pairs in |cfstr_id_pairs| to do the conversion (by doing a
// linear lookup).
template <typename IdType, typename StringIdPair>
bool GetImageBufferProperty(CVImageBufferRef image_buffer,
                            CFStringRef key,
                            const std::vector<StringIdPair>& cfstr_id_pairs,
                            IdType* value_as_id) {
  CFStringRef value_as_string = reinterpret_cast<CFStringRef>(
      CVBufferGetAttachment(image_buffer, key, nullptr));
  if (!value_as_string)
    return false;

  for (const auto& p : cfstr_id_pairs) {
    if (!CFStringCompare(value_as_string, p.cfstr, 0)) {
      *value_as_id = p.id;
      return true;
    }
  }

  return false;
}

gfx::ColorSpace::PrimaryID GetImageBufferPrimary(
    CVImageBufferRef image_buffer) {
  struct CVImagePrimary {
    const CFStringRef cfstr;
    const gfx::ColorSpace::PrimaryID id;
  };
  static const base::NoDestructor<std::vector<CVImagePrimary>>
      kSupportedPrimaries([] {
        std::vector<CVImagePrimary> supported_primaries;
        supported_primaries.push_back({kCVImageBufferColorPrimaries_ITU_R_709_2,
                                       gfx::ColorSpace::PrimaryID::BT709});
        supported_primaries.push_back({kCVImageBufferColorPrimaries_EBU_3213,
                                       gfx::ColorSpace::PrimaryID::BT470BG});
        supported_primaries.push_back({kCVImageBufferColorPrimaries_SMPTE_C,
                                       gfx::ColorSpace::PrimaryID::SMPTE240M});
        if (@available(macos 10.11, *)) {
          supported_primaries.push_back(
              {kCVImageBufferColorPrimaries_ITU_R_2020,
               gfx::ColorSpace::PrimaryID::BT2020});
        }
        return supported_primaries;
      }());

  // The named primaries. Default to BT709.
  auto primary_id = gfx::ColorSpace::PrimaryID::BT709;
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferColorPrimariesKey,
                              *kSupportedPrimaries, &primary_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef primaries.";
  }
  return primary_id;
}

gfx::ColorSpace::TransferID GetImageBufferTransferFn(
    CVImageBufferRef image_buffer,
    double* gamma) {
  struct CVImageTransferFn {
    const CFStringRef cfstr;
    const gfx::ColorSpace::TransferID id;
  };
  static const base::NoDestructor<std::vector<CVImageTransferFn>>
      kSupportedTransferFuncs([] {
        std::vector<CVImageTransferFn> supported_transfer_funcs;
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_709_2,
             gfx::ColorSpace::TransferID::BT709_APPLE});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_240M_1995,
             gfx::ColorSpace::TransferID::SMPTE240M});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_UseGamma,
             gfx::ColorSpace::TransferID::CUSTOM});
        if (@available(macos 10.11, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_ITU_R_2020,
               gfx::ColorSpace::TransferID::BT2020_10});
        }
        if (@available(macos 10.12, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_SMPTE_ST_428_1,
               gfx::ColorSpace::TransferID::SMPTEST428_1});
        }
        if (@available(macos 10.13, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
               gfx::ColorSpace::TransferID::SMPTEST2084});
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_ITU_R_2100_HLG,
               gfx::ColorSpace::TransferID::ARIB_STD_B67});
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_sRGB,
               gfx::ColorSpace::TransferID::IEC61966_2_1});
        }
        if (@available(macos 10.14, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_Linear,
               gfx::ColorSpace::TransferID::LINEAR});
        }

        return supported_transfer_funcs;
      }());

  // The named transfer function.
  auto transfer_id = gfx::ColorSpace::TransferID::BT709;
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferTransferFunctionKey,
                              *kSupportedTransferFuncs, &transfer_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef transfer.";
  }

  if (transfer_id != gfx::ColorSpace::TransferID::CUSTOM)
    return transfer_id;

  // If we fail to retrieve the gamma parameter, fall back to BT709.
  constexpr auto kDefaultTransferFn = gfx::ColorSpace::TransferID::BT709;
  CFNumberRef gamma_number =
      reinterpret_cast<CFNumberRef>(CVBufferGetAttachment(
          image_buffer, kCVImageBufferGammaLevelKey, nullptr));
  if (!gamma_number) {
    DLOG(ERROR) << "Failed to get CVImageBufferRef gamma level.";
    return kDefaultTransferFn;
  }

  // CGFloat is a double on 64-bit systems.
  CGFloat gamma_double = 0;
  if (!CFNumberGetValue(gamma_number, kCFNumberCGFloatType, &gamma_double)) {
    DLOG(ERROR) << "Failed to get CVImageBufferRef gamma level as float.";
    return kDefaultTransferFn;
  }

  if (gamma_double == 2.2)
    return gfx::ColorSpace::TransferID::GAMMA22;
  if (gamma_double == 2.8)
    return gfx::ColorSpace::TransferID::GAMMA28;

  *gamma = gamma_double;
  return transfer_id;
}

gfx::ColorSpace::MatrixID GetImageBufferMatrix(CVImageBufferRef image_buffer) {
  struct CVImageMatrix {
    const CFStringRef cfstr;
    gfx::ColorSpace::MatrixID id;
  };
  static const base::NoDestructor<std::vector<CVImageMatrix>>
      kSupportedMatrices([] {
        std::vector<CVImageMatrix> supported_matrices;
        supported_matrices.push_back({kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                                      gfx::ColorSpace::MatrixID::BT709});
        supported_matrices.push_back({kCVImageBufferYCbCrMatrix_ITU_R_601_4,
                                      gfx::ColorSpace::MatrixID::SMPTE170M});
        supported_matrices.push_back({kCVImageBufferYCbCrMatrix_SMPTE_240M_1995,
                                      gfx::ColorSpace::MatrixID::SMPTE240M});
        if (@available(macos 10.11, *)) {
          supported_matrices.push_back({kCVImageBufferYCbCrMatrix_ITU_R_2020,
                                        gfx::ColorSpace::MatrixID::BT2020_NCL});
        }
        return supported_matrices;
      }());

  auto matrix_id = gfx::ColorSpace::MatrixID::INVALID;
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferYCbCrMatrixKey,
                              *kSupportedMatrices, &matrix_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef YUV matrix.";
  }
  return matrix_id;
}

void SetVp9CodecConfigurationBox(
    media::VideoCodecProfile codec_profile,
    const media::VideoColorSpace& color_space,
    NSMutableDictionary<NSString*, id>* extensions) {
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

}  // namespace

namespace media {

CFMutableDictionaryRef CreateFormatExtensions(
    CMVideoCodecType codec_type,
    VideoCodecProfile profile,
    const VideoColorSpace& color_space,
    base::Optional<gl::HDRMetadata> hdr_metadata) {
  auto* extensions = [[NSMutableDictionary alloc] init];
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

  if (hdr_metadata) {
    SetContentLightLevelInfo(*hdr_metadata, extensions);
    SetMasteringMetadata(*hdr_metadata, extensions);
  }

  if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX)
    SetVp9CodecConfigurationBox(profile, color_space, extensions);

  return base::mac::NSToCFCast(extensions);
}

gfx::ColorSpace GetImageBufferColorSpace(CVImageBufferRef image_buffer) {
  double gamma;
  auto primary_id = GetImageBufferPrimary(image_buffer);
  auto matrix_id = GetImageBufferMatrix(image_buffer);
  auto transfer_id = GetImageBufferTransferFn(image_buffer, &gamma);

  // Use a matrix id that is coherent with a primary id. Useful when we fail to
  // parse the matrix. Previously it was always defaulting to MatrixID::BT709
  // See http://crbug.com/788236.
  if (matrix_id == gfx::ColorSpace::MatrixID::INVALID) {
    if (primary_id == gfx::ColorSpace::PrimaryID::BT470BG)
      matrix_id = gfx::ColorSpace::MatrixID::BT470BG;
    else
      matrix_id = gfx::ColorSpace::MatrixID::BT709;
  }

  // It is specified to the decoder to use luma=[16,235] chroma=[16,240] via
  // the kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange.
  //
  // TODO(crbug.com/1103432): We'll probably need support for more than limited
  // range content if we want this to be used for more than video sites.
  auto range_id = gfx::ColorSpace::RangeID::LIMITED;

  if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM) {
    // Transfer functions can also be specified as a gamma value.
    skcms_TransferFunction custom_tr_fn = {2.2f, 1, 0, 1, 0, 0, 0};
    if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM)
      custom_tr_fn.g = gamma;

    return gfx::ColorSpace(primary_id, gfx::ColorSpace::TransferID::CUSTOM,
                           matrix_id, range_id, nullptr, &custom_tr_fn);
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

}  // namespace media
