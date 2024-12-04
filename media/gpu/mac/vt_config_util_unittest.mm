// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/mac/vt_config_util.h"

#include <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/containers/span.h"
#include "base/mac/mac_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/formats/mp4/box_definitions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/hdr_metadata_mac.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFPtrCast;

namespace {

std::string GetStrValue(CFDictionaryRef dict, CFStringRef key) {
  return base::SysCFStringRefToUTF8(
      base::apple::CFCastStrict<CFStringRef>(CFDictionaryGetValue(dict, key)));
}

CFStringRef GetCFStrValue(CFDictionaryRef dict, CFStringRef key) {
  return base::apple::CFCastStrict<CFStringRef>(
      CFDictionaryGetValue(dict, key));
}

int GetIntValue(CFDictionaryRef dict, CFStringRef key) {
  CFNumberRef value =
      base::apple::CFCastStrict<CFNumberRef>(CFDictionaryGetValue(dict, key));
  int result;
  return CFNumberGetValue(value, kCFNumberIntType, &result) ? result : -1;
}

bool GetBoolValue(CFDictionaryRef dict, CFStringRef key) {
  return CFBooleanGetValue(
      base::apple::CFCastStrict<CFBooleanRef>(CFDictionaryGetValue(dict, key)));
}

base::span<const uint8_t> GetDataValue(CFDictionaryRef dict, CFStringRef key) {
  CFDataRef data =
      base::apple::CFCastStrict<CFDataRef>(CFDictionaryGetValue(dict, key));
  return data ? base::apple::CFDataToSpan(data) : base::span<const uint8_t>();
}

base::span<const uint8_t> GetNestedDataValue(CFDictionaryRef dict,
                                             CFStringRef key1,
                                             CFStringRef key2) {
  CFDictionaryRef nested_dict = base::apple::CFCastStrict<CFDictionaryRef>(
      CFDictionaryGetValue(dict, key1));
  return GetDataValue(nested_dict, key2);
}

base::apple::ScopedCFTypeRef<CVImageBufferRef> CreateCVImageBuffer(
    media::VideoColorSpace cs) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt =
      CreateFormatExtensions(kCMVideoCodecType_H264, media::H264PROFILE_MAIN, 8,
                             cs, gfx::HDRMetadata(), std::nullopt);

  base::apple::ScopedCFTypeRef<CVImageBufferRef> image_buffer;
  OSStatus err =
      CVPixelBufferCreate(kCFAllocatorDefault, 16, 16,
                          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                          nullptr, image_buffer.InitializeInto());
  if (err != noErr) {
    EXPECT_EQ(err, noErr);
    return base::apple::ScopedCFTypeRef<CVImageBufferRef>();
  }

  CVBufferSetAttachments(image_buffer.get(), fmt.get(),
                         kCVAttachmentMode_ShouldNotPropagate);
  return image_buffer;
}

base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> CreateFormatDescription(
    CFStringRef primaries,
    CFStringRef transfer,
    CFStringRef matrix) {
  NSMutableDictionary* extensions = [NSMutableDictionary dictionary];

  if (primaries) {
    extensions[CFToNSPtrCast(kCMFormatDescriptionExtension_ColorPrimaries)] =
        CFToNSPtrCast(primaries);
  }
  if (transfer) {
    extensions[CFToNSPtrCast(kCMFormatDescriptionExtension_TransferFunction)] =
        CFToNSPtrCast(transfer);
  }
  if (matrix) {
    extensions[CFToNSPtrCast(kCMFormatDescriptionExtension_YCbCrMatrix)] =
        CFToNSPtrCast(matrix);
  }
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> result;
  CMFormatDescriptionCreate(nullptr, kCMMediaType_Video,
                            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                            NSToCFPtrCast(extensions), result.InitializeInto());
  return result;
}

gfx::ColorSpace ToBT709_APPLE(gfx::ColorSpace cs) {
  return gfx::ColorSpace(cs.GetPrimaryID(),
                         gfx::ColorSpace::TransferID::BT709_APPLE,
                         cs.GetMatrixID(), cs.GetRangeID());
}

void AssertHasDefaultHDRMetadata(CFDictionaryRef fmt) {
  // We constructed with an invalid HDRMetadata, so all values should be
  // overridden to the default.
  auto mdcv_expected = gfx::GenerateMasteringDisplayColorVolume(std::nullopt);
  auto clli_expected = gfx::GenerateContentLightLevelInfo(std::nullopt);

  auto mdcv = GetDataValue(
      fmt, kCMFormatDescriptionExtension_MasteringDisplayColorVolume);
  ASSERT_EQ(24u, mdcv.size());
  ASSERT_EQ(24u, CFDataGetLength(mdcv_expected.get()));
  EXPECT_EQ(0, memcmp(mdcv.data(), CFDataGetBytePtr(mdcv_expected.get()), 24u));

  auto clli =
      GetDataValue(fmt, kCMFormatDescriptionExtension_ContentLightLevelInfo);
  ASSERT_EQ(0u, clli.size());
}

void AssertHasNoHDRMetadata(CFDictionaryRef fmt) {
  auto mdcv = GetDataValue(
      fmt, kCMFormatDescriptionExtension_MasteringDisplayColorVolume);
  auto clli =
      GetDataValue(fmt, kCMFormatDescriptionExtension_ContentLightLevelInfo);
  EXPECT_TRUE(mdcv.empty());
  EXPECT_TRUE(clli.empty());
}

constexpr char kBitDepthKey[] = "BitsPerComponent";
constexpr char kVpccKey[] = "vpcC";

}  // namespace

namespace media {

TEST(VTConfigUtil, CreateFormatExtensions_H264_BT709) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt = CreateFormatExtensions(
      kCMVideoCodecType_H264, H264PROFILE_MAIN, 8, VideoColorSpace::REC709(),
      std::nullopt, std::nullopt);

  EXPECT_EQ("avc1",
            GetStrValue(fmt.get(), kCMFormatDescriptionExtension_FormatName));
  EXPECT_EQ(24, GetIntValue(fmt.get(), kCMFormatDescriptionExtension_Depth));
  EXPECT_EQ(
      kCMFormatDescriptionColorPrimaries_ITU_R_709_2,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_ColorPrimaries));
  EXPECT_EQ(
      kCMFormatDescriptionTransferFunction_ITU_R_709_2,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_TransferFunction));
  EXPECT_EQ(
      kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_YCbCrMatrix));
  EXPECT_FALSE(
      GetBoolValue(fmt.get(), kCMFormatDescriptionExtension_FullRangeVideo));
  EXPECT_TRUE(
      GetDataValue(fmt.get(),
                   kCMFormatDescriptionExtension_MasteringDisplayColorVolume)
          .empty());
  EXPECT_TRUE(GetDataValue(fmt.get(),
                           kCMFormatDescriptionExtension_ContentLightLevelInfo)
                  .empty());
}

TEST(VTConfigUtil, CreateFormatExtensions_H264_BT2020_PQ) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt = CreateFormatExtensions(
      kCMVideoCodecType_H264, H264PROFILE_MAIN, 8,
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::SMPTEST2084,
                      VideoColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::FULL),
      gfx::HDRMetadata(), std::nullopt);

  EXPECT_EQ("avc1",
            GetStrValue(fmt.get(), kCMFormatDescriptionExtension_FormatName));
  EXPECT_EQ(24, GetIntValue(fmt.get(), kCMFormatDescriptionExtension_Depth));
  EXPECT_EQ(
      kCMFormatDescriptionColorPrimaries_ITU_R_2020,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_ColorPrimaries));
  EXPECT_EQ(
      kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_TransferFunction));
  EXPECT_EQ(
      kCMFormatDescriptionYCbCrMatrix_ITU_R_2020,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_YCbCrMatrix));
  EXPECT_TRUE(
      GetBoolValue(fmt.get(), kCMFormatDescriptionExtension_FullRangeVideo));
  AssertHasDefaultHDRMetadata(fmt.get());
}

TEST(VTConfigUtil, CreateFormatExtensions_H264_BT2020_HLG) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt = CreateFormatExtensions(
      kCMVideoCodecType_H264, H264PROFILE_MAIN, 8,
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::ARIB_STD_B67,
                      VideoColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::FULL),
      gfx::HDRMetadata(), std::nullopt);

  EXPECT_EQ("avc1",
            GetStrValue(fmt.get(), kCMFormatDescriptionExtension_FormatName));
  EXPECT_EQ(24, GetIntValue(fmt.get(), kCMFormatDescriptionExtension_Depth));
  EXPECT_EQ(
      kCMFormatDescriptionColorPrimaries_ITU_R_2020,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_ColorPrimaries));
  EXPECT_EQ(
      kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_TransferFunction));
  EXPECT_EQ(
      kCMFormatDescriptionYCbCrMatrix_ITU_R_2020,
      GetCFStrValue(fmt.get(), kCMFormatDescriptionExtension_YCbCrMatrix));
  EXPECT_TRUE(
      GetBoolValue(fmt.get(), kCMFormatDescriptionExtension_FullRangeVideo));
  AssertHasNoHDRMetadata(fmt.get());
}

TEST(VTConfigUtil, CreateFormatExtensions_HDRMetadata) {
  // Values from real YouTube HDR content.
  gfx::HDRMetadata hdr_meta;
  hdr_meta.cta_861_3 = gfx::HdrMetadataCta861_3(1000, 600);
  hdr_meta.smpte_st_2086 = gfx::HdrMetadataSmpteSt2086(
      {0.6800f, 0.3200f, 0.2649f, 0.6900f, 0.1500f, 0.0600f, 0.3127f, 0.3290f},
      /*luminance_max=*/1000,
      /*luminance_min=*/0);
  const auto& cv_metadata = hdr_meta.smpte_st_2086.value();

  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt = CreateFormatExtensions(
      kCMVideoCodecType_H264, H264PROFILE_MAIN, 8,
      VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                      VideoColorSpace::TransferID::SMPTEST2084,
                      VideoColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::FULL),
      hdr_meta, std::nullopt);

  {
    auto mdcv = GetDataValue(
        fmt.get(), kCMFormatDescriptionExtension_MasteringDisplayColorVolume);
    ASSERT_EQ(24u, mdcv.size());
    std::unique_ptr<mp4::BoxReader> box_reader(
        mp4::BoxReader::ReadConcatentatedBoxes(mdcv.data(), mdcv.size(),
                                               nullptr));
    mp4::MasteringDisplayColorVolume mdcv_box;
    ASSERT_TRUE(mdcv_box.Parse(box_reader.get()));
    EXPECT_EQ(mdcv_box.display_primaries_gx, cv_metadata.primaries.fGX);
    EXPECT_EQ(mdcv_box.display_primaries_gy, cv_metadata.primaries.fGY);
    EXPECT_EQ(mdcv_box.display_primaries_bx, cv_metadata.primaries.fBX);
    EXPECT_EQ(mdcv_box.display_primaries_by, cv_metadata.primaries.fBY);
    EXPECT_EQ(mdcv_box.display_primaries_rx, cv_metadata.primaries.fRX);
    EXPECT_EQ(mdcv_box.display_primaries_ry, cv_metadata.primaries.fRY);
    EXPECT_EQ(mdcv_box.white_point_x, cv_metadata.primaries.fWX);
    EXPECT_EQ(mdcv_box.white_point_y, cv_metadata.primaries.fWY);
    EXPECT_EQ(mdcv_box.max_display_mastering_luminance,
              cv_metadata.luminance_max);
    EXPECT_EQ(mdcv_box.min_display_mastering_luminance,
              cv_metadata.luminance_min);
  }

  {
    auto clli = GetDataValue(
        fmt.get(), kCMFormatDescriptionExtension_ContentLightLevelInfo);
    ASSERT_EQ(4u, clli.size());
    std::unique_ptr<mp4::BoxReader> box_reader(
        mp4::BoxReader::ReadConcatentatedBoxes(clli.data(), clli.size(),
                                               nullptr));
    mp4::ContentLightLevelInformation clli_box;
    ASSERT_TRUE(clli_box.Parse(box_reader.get()));
    EXPECT_EQ(clli_box.max_content_light_level,
              hdr_meta.cta_861_3->max_content_light_level);
    EXPECT_EQ(clli_box.max_pic_average_light_level,
              hdr_meta.cta_861_3->max_frame_average_light_level);
  }
}

TEST(VTConfigUtil, CreateFormatExtensions_VP9Profile0) {
  constexpr VideoCodecProfile kTestProfile = VP9PROFILE_PROFILE0;
  const auto kTestColorSpace = VideoColorSpace::REC709();
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt =
      CreateFormatExtensions(kCMVideoCodecType_VP9, kTestProfile, 8,
                             kTestColorSpace, std::nullopt, std::nullopt);
  EXPECT_EQ(8, GetIntValue(fmt.get(),
                           base::SysUTF8ToCFStringRef(kBitDepthKey).get()));

  auto vpcc = GetNestedDataValue(
      fmt.get(), kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      base::SysUTF8ToCFStringRef(kVpccKey).get());
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(vpcc.data(), vpcc.size(),
                                             nullptr));
  mp4::VPCodecConfigurationRecord vpcc_box;
  ASSERT_TRUE(vpcc_box.Parse(box_reader.get()));
  ASSERT_EQ(kTestProfile, vpcc_box.profile);
  ASSERT_EQ(kTestColorSpace, vpcc_box.color_space);
}

TEST(VTConfigUtil, CreateFormatExtensions_VP9Profile2) {
  constexpr VideoCodecProfile kTestProfile = VP9PROFILE_PROFILE2;
  const VideoColorSpace kTestColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::SMPTEST2084,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt =
      CreateFormatExtensions(kCMVideoCodecType_VP9, kTestProfile, 10,
                             kTestColorSpace, std::nullopt, std::nullopt);
  EXPECT_EQ(10, GetIntValue(fmt.get(),
                            base::SysUTF8ToCFStringRef(kBitDepthKey).get()));

  auto vpcc = GetNestedDataValue(
      fmt.get(), kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      base::SysUTF8ToCFStringRef(kVpccKey).get());
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(vpcc.data(), vpcc.size(),
                                             nullptr));
  mp4::VPCodecConfigurationRecord vpcc_box;
  ASSERT_TRUE(vpcc_box.Parse(box_reader.get()));
  ASSERT_EQ(kTestProfile, vpcc_box.profile);
  ASSERT_EQ(kTestColorSpace, vpcc_box.color_space);
}

TEST(VTConfigUtil, CreateFormatExtensions_AV1) {
  // Dumped from a main profile 10-bit AV1 stream.
  constexpr uint8_t kAvc1Box[] = {0x81, 0x04, 0x4c, 0x00, 0x0a, 0x0b,
                                  0x00, 0x00, 0x00, 0x24, 0xcf, 0x7f,
                                  0x0d, 0xbf, 0xff, 0x38, 0x08};

  constexpr VideoCodecProfile kTestProfile = AV1PROFILE_PROFILE_MAIN;
  const VideoColorSpace kTestColorSpace(
      VideoColorSpace::PrimaryID::BT2020,
      VideoColorSpace::TransferID::SMPTEST2084,
      VideoColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
  base::apple::ScopedCFTypeRef<CFDictionaryRef> fmt = CreateFormatExtensions(
      kCMVideoCodecType_AV1, kTestProfile, 10, kTestColorSpace, std::nullopt,
      base::span<const uint8_t>(kAvc1Box, sizeof(kAvc1Box)));
  EXPECT_EQ(10, GetIntValue(fmt.get(),
                            base::SysUTF8ToCFStringRef(kBitDepthKey).get()));

  auto av1c = GetNestedDataValue(
      fmt.get(), kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
      base::SysUTF8ToCFStringRef("av1C").get());
  std::unique_ptr<mp4::BoxReader> box_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(av1c.data(), av1c.size(),
                                             nullptr));
  mp4::AV1CodecConfigurationRecord av1c_box;
  ASSERT_TRUE(av1c_box.Parse(box_reader.get()));
  ASSERT_EQ(kTestProfile, av1c_box.profile);
  // No other fields are parsed by mp4::AV1CodecConfigurationRecord.
}

TEST(VTConfigUtil, GetImageBufferColorSpace_BT601) {
  auto cs = VideoColorSpace::REC601();
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);

  cs.primaries = VideoColorSpace::PrimaryID::SMPTE170M;
  auto expected_cs = ToBT709_APPLE(cs.ToGfxColorSpace());
  EXPECT_EQ(expected_cs, GetImageBufferColorSpace(image_buffer.get()));
}

TEST(VTConfigUtil, GetImageBufferColorSpace_BT709) {
  auto cs = VideoColorSpace::REC709();
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);

  // macOS returns a special BT709_APPLE transfer function since it doesn't use
  // the same gamma level as is standardized.
  auto expected_cs = ToBT709_APPLE(cs.ToGfxColorSpace());
  EXPECT_EQ(expected_cs, GetImageBufferColorSpace(image_buffer.get()));
}

TEST(VTConfigUtil, GetImageBufferColorSpace_GAMMA22) {
  auto cs = VideoColorSpace(VideoColorSpace::PrimaryID::SMPTE170M,
                            VideoColorSpace::TransferID::GAMMA22,
                            VideoColorSpace::MatrixID::SMPTE170M,
                            gfx::ColorSpace::RangeID::LIMITED);
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);
  EXPECT_EQ(cs.ToGfxColorSpace(), GetImageBufferColorSpace(image_buffer.get()));
}

TEST(VTConfigUtil, GetImageBufferColorSpace_GAMMA28) {
  auto cs = VideoColorSpace(VideoColorSpace::PrimaryID::SMPTE170M,
                            VideoColorSpace::TransferID::GAMMA28,
                            VideoColorSpace::MatrixID::SMPTE170M,
                            gfx::ColorSpace::RangeID::LIMITED);
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);
  EXPECT_EQ(cs.ToGfxColorSpace(), GetImageBufferColorSpace(image_buffer.get()));
}

TEST(VTConfigUtil, GetImageBufferColorSpace_BT2020_PQ) {
  auto cs = VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                            VideoColorSpace::TransferID::SMPTEST2084,
                            VideoColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED);
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);
  auto image_buffer_cs = GetImageBufferColorSpace(image_buffer.get());

  // When BT.2020 is unavailable the default should be BT.709.
  EXPECT_EQ(cs.ToGfxColorSpace(), image_buffer_cs);
}

TEST(VTConfigUtil, GetImageBufferColorSpace_BT2020_HLG) {
  auto cs = VideoColorSpace(VideoColorSpace::PrimaryID::BT2020,
                            VideoColorSpace::TransferID::ARIB_STD_B67,
                            VideoColorSpace::MatrixID::BT2020_NCL,
                            gfx::ColorSpace::RangeID::LIMITED);
  auto image_buffer = CreateCVImageBuffer(cs);
  ASSERT_TRUE(image_buffer);
  auto image_buffer_cs = GetImageBufferColorSpace(image_buffer.get());

  // When BT.2020 is unavailable the default should be BT.709.
  EXPECT_EQ(cs.ToGfxColorSpace(), image_buffer_cs);
}

TEST(VTConfigUtil, FormatDescriptionInvalid) {
  auto format_descriptor =
      CreateFormatDescription(CFSTR("Cows"), CFSTR("Go"), CFSTR("Moo"));
  ASSERT_TRUE(format_descriptor);
  auto cs = GetFormatDescriptionColorSpace(format_descriptor.get());
  EXPECT_FALSE(cs.IsValid());
}

TEST(VTConfigUtil, FormatDescriptionBT709) {
  auto format_descriptor =
      CreateFormatDescription(kCMFormatDescriptionColorPrimaries_ITU_R_709_2,
                              kCMFormatDescriptionTransferFunction_ITU_R_709_2,
                              kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2);
  ASSERT_TRUE(format_descriptor);
  auto cs = GetFormatDescriptionColorSpace(format_descriptor.get());
  EXPECT_EQ(ToBT709_APPLE(gfx::ColorSpace::CreateREC709()), cs);
}

}  // namespace media
