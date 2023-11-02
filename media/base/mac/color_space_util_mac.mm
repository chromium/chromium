// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/color_space_util_mac.h"

#include <simd/simd.h>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace media {

namespace {

// Read the value for the key in |key| to CFString and convert it to IdType.
// Use the list of pairs in |cfstr_id_pairs| to do the conversion (by doing a
// linear lookup).
template <typename IdType, typename StringIdPair>
bool GetImageBufferProperty(CFTypeRef value_untyped,
                            const std::vector<StringIdPair>& cfstr_id_pairs,
                            IdType* value_as_id) {
  CFStringRef value_as_string = base::mac::CFCast<CFStringRef>(value_untyped);
  if (!value_as_string)
    return false;

  for (const auto& p : cfstr_id_pairs) {
    if (p.cfstr_cm)
      DCHECK(!CFStringCompare(p.cfstr_cv, p.cfstr_cm, 0));
    if (!CFStringCompare(value_as_string, p.cfstr_cv, 0)) {
      *value_as_id = p.id;
      return true;
    }
  }

  return false;
}

gfx::ColorSpace::PrimaryID GetCoreVideoPrimary(CFTypeRef primaries_untyped) {
  struct CVImagePrimary {
    const CFStringRef cfstr_cv;
    const CFStringRef cfstr_cm;
    const gfx::ColorSpace::PrimaryID id;
  };
  static const base::NoDestructor<std::vector<CVImagePrimary>>
      kSupportedPrimaries([] {
        std::vector<CVImagePrimary> supported_primaries;
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_ITU_R_709_2,
             kCMFormatDescriptionColorPrimaries_ITU_R_709_2,
             gfx::ColorSpace::PrimaryID::BT709});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_EBU_3213,
             kCMFormatDescriptionColorPrimaries_EBU_3213,
             gfx::ColorSpace::PrimaryID::BT470BG});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_SMPTE_C,
             kCMFormatDescriptionColorPrimaries_SMPTE_C,
             gfx::ColorSpace::PrimaryID::SMPTE240M});
        supported_primaries.push_back(
            {kCVImageBufferColorPrimaries_ITU_R_2020,
             kCMFormatDescriptionColorPrimaries_ITU_R_2020,
             gfx::ColorSpace::PrimaryID::BT2020});
        return supported_primaries;
      }());

  // The named primaries. Default to BT709.
  auto primary_id = gfx::ColorSpace::PrimaryID::BT709;
  if (!GetImageBufferProperty(primaries_untyped, *kSupportedPrimaries,
                              &primary_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef primaries.";
  }
  return primary_id;
}

gfx::ColorSpace::TransferID GetCoreVideoTransferFn(CFTypeRef transfer_untyped,
                                                   CFTypeRef gamma_untyped,
                                                   double* gamma) {
  struct CVImageTransferFn {
    const CFStringRef cfstr_cv;
    const CFStringRef cfstr_cm;
    const gfx::ColorSpace::TransferID id;
  };
  static const base::NoDestructor<std::vector<CVImageTransferFn>>
      kSupportedTransferFuncs([] {
        std::vector<CVImageTransferFn> supported_transfer_funcs;
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_709_2,
             kCMFormatDescriptionTransferFunction_ITU_R_709_2,
             gfx::ColorSpace::TransferID::BT709_APPLE});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_240M_1995,
             kCMFormatDescriptionTransferFunction_SMPTE_240M_1995,
             gfx::ColorSpace::TransferID::SMPTE240M});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_UseGamma,
             kCMFormatDescriptionTransferFunction_UseGamma,
             gfx::ColorSpace::TransferID::CUSTOM});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_2020,
             kCMFormatDescriptionTransferFunction_ITU_R_2020,
             gfx::ColorSpace::TransferID::BT2020_10});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_ST_428_1,
             kCMFormatDescriptionTransferFunction_SMPTE_ST_428_1,
             gfx::ColorSpace::TransferID::SMPTEST428_1});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
             kCMFormatDescriptionTransferFunction_SMPTE_ST_2084_PQ,
             gfx::ColorSpace::TransferID::PQ});
        supported_transfer_funcs.push_back(
            {kCVImageBufferTransferFunction_ITU_R_2100_HLG,
             kCMFormatDescriptionTransferFunction_ITU_R_2100_HLG,
             gfx::ColorSpace::TransferID::HLG});
        supported_transfer_funcs.push_back({kCVImageBufferTransferFunction_sRGB,
                                            nullptr,
                                            gfx::ColorSpace::TransferID::SRGB});
        if (@available(macos 10.14, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_Linear,
               kCMFormatDescriptionTransferFunction_Linear,
               gfx::ColorSpace::TransferID::LINEAR});
        }
        if (@available(macos 10.15, *)) {
          supported_transfer_funcs.push_back(
              {kCVImageBufferTransferFunction_sRGB,
               kCMFormatDescriptionTransferFunction_sRGB,
               gfx::ColorSpace::TransferID::SRGB});
        }

        return supported_transfer_funcs;
      }());

  // The named transfer function.
  auto transfer_id = gfx::ColorSpace::TransferID::BT709;
  if (!GetImageBufferProperty(transfer_untyped, *kSupportedTransferFuncs,
                              &transfer_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef transfer.";
  }

  if (transfer_id != gfx::ColorSpace::TransferID::CUSTOM)
    return transfer_id;

  // If we fail to retrieve the gamma parameter, fall back to BT709.
  constexpr auto kDefaultTransferFn = gfx::ColorSpace::TransferID::BT709;
  CFNumberRef gamma_number = base::mac::CFCast<CFNumberRef>(gamma_untyped);
  if (!gamma_number) {
    DLOG(ERROR) << "Failed to get gamma level.";
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

gfx::ColorSpace::MatrixID GetCoreVideoMatrix(CFTypeRef matrix_untyped) {
  struct CVImageMatrix {
    const CFStringRef cfstr_cv;
    const CFStringRef cfstr_cm;
    gfx::ColorSpace::MatrixID id;
  };
  static const base::NoDestructor<std::vector<CVImageMatrix>>
      kSupportedMatrices([] {
        std::vector<CVImageMatrix> supported_matrices;
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_709_2,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_709_2,
             gfx::ColorSpace::MatrixID::BT709});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_601_4,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_601_4,
             gfx::ColorSpace::MatrixID::SMPTE170M});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_SMPTE_240M_1995,
             kCMFormatDescriptionYCbCrMatrix_SMPTE_240M_1995,
             gfx::ColorSpace::MatrixID::SMPTE240M});
        supported_matrices.push_back(
            {kCVImageBufferYCbCrMatrix_ITU_R_2020,
             kCMFormatDescriptionYCbCrMatrix_ITU_R_2020,
             gfx::ColorSpace::MatrixID::BT2020_NCL});
        return supported_matrices;
      }());

  auto matrix_id = gfx::ColorSpace::MatrixID::INVALID;
  if (!GetImageBufferProperty(matrix_untyped, *kSupportedMatrices,
                              &matrix_id)) {
    DLOG(ERROR) << "Failed to find CVImageBufferRef YUV matrix.";
  }
  return matrix_id;
}

gfx::ColorSpace GetCoreVideoColorSpaceInternal(CFTypeRef primaries_untyped,
                                               CFTypeRef transfer_untyped,
                                               CFTypeRef gamma_untyped,
                                               CFTypeRef matrix_untyped) {
  double gamma;
  auto primary_id = GetCoreVideoPrimary(primaries_untyped);
  auto matrix_id = GetCoreVideoMatrix(matrix_untyped);
  auto transfer_id =
      GetCoreVideoTransferFn(transfer_untyped, gamma_untyped, &gamma);

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

}  // anonymous namespace

gfx::ColorSpace GetImageBufferColorSpace(CVImageBufferRef image_buffer) {
  return GetCoreVideoColorSpaceInternal(
      CVBufferGetAttachment(image_buffer, kCVImageBufferColorPrimariesKey,
                            nullptr),
      CVBufferGetAttachment(image_buffer, kCVImageBufferTransferFunctionKey,
                            nullptr),
      CVBufferGetAttachment(image_buffer, kCVImageBufferGammaLevelKey, nullptr),
      CVBufferGetAttachment(image_buffer, kCVImageBufferYCbCrMatrixKey,
                            nullptr));
}

gfx::ColorSpace GetFormatDescriptionColorSpace(
    CMFormatDescriptionRef format_description) {
  return GetCoreVideoColorSpaceInternal(
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_ColorPrimaries),
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_TransferFunction),
      CMFormatDescriptionGetExtension(format_description,
                                      kCMFormatDescriptionExtension_GammaLevel),
      CMFormatDescriptionGetExtension(
          format_description, kCMFormatDescriptionExtension_YCbCrMatrix));
}

}  // namespace media
