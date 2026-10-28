// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"

#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/hdr_metadata.h"

namespace blink {

const ImageEncodingMimeType ImageEncoderUtils::kDefaultEncodingMimeType =
    kMimeTypePng;
const char ImageEncoderUtils::kDefaultRequestedMimeType[] = "image/png";

ImageEncodingMimeType ImageEncoderUtils::ToEncodingMimeType(
    const String& mime_type_name) {
  String lowercase_mime_type = mime_type_name.ToAsciiLower();
  if (mime_type_name.IsNull()) {
    lowercase_mime_type = kDefaultRequestedMimeType;
  }

  ImageEncodingMimeType encoding_mime_type = kDefaultEncodingMimeType;
  // FIXME: Make isSupportedImageMIMETypeForEncoding threadsafe (to allow this
  // method to be used on a worker thread).
  if (MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(
          lowercase_mime_type)) {
    ParseMimeType(lowercase_mime_type, encoding_mime_type);
  }
  return encoding_mime_type;
}

bool ImageEncoderUtils::ParseMimeType(const String& mime_type_name,
                                      ImageEncodingMimeType& mime_type) {
  if (mime_type_name == "image/png") {
    mime_type = kMimeTypePng;
  } else if (mime_type_name == "image/jpeg") {
    mime_type = kMimeTypeJpeg;
  } else if (mime_type_name == "image/webp") {
    mime_type = kMimeTypeWebp;
  } else {
    return false;
  }
  return true;
}

String ImageEncoderUtils::MimeTypeName(ImageEncodingMimeType mime_type) {
  DCHECK_GE(mime_type, 0);
  DCHECK_LT(mime_type, 3);
  constexpr std::array<const char* const, 3> kMimeTypeNames = {
      "image/png", "image/jpeg", "image/webp"};
  return kMimeTypeNames[mime_type];
}

SkColorInfo ImageEncoderUtils::GetColorInfoForEncoder(
    const SkColorInfo& color_info,
    const gfx::HDRMetadata& hdr_metadata) {
  SkColorType color_type = color_info.colorType();
  // Convert premultiplied or opaque alpha to unpremultiplied.
  // TODO(crbug.com/454152417): Avoid converting kOpaque_SkAlphaType to
  // kUnpremul_SkAlphaType, and update tests that assert string equality on
  // data URLs between 2D and bitmaprenderer canvases (e.g.
  // http/tests/canvas/toDataURL-clean-canvas.html).
  SkAlphaType alpha_type = kUnpremul_SkAlphaType;
  sk_sp<SkColorSpace> color_space = color_info.refColorSpace();

  // Only float images can represent values outside of [0, 1].
  if (color_space && color_type == kRGBA_F16_SkColorType) {
    float baseline_headroom = 0.f;
    float hdr_reference_white =
        skhdr::AdaptiveGlobalToneMap::kDefaultHdrReferenceWhite;
    if (hdr_metadata.HasAgtm()) {
      const auto& agtm = hdr_metadata.GetAgtm();
      hdr_reference_white = agtm.fHdrReferenceWhite;
      if (agtm.fHeadroomAdaptiveToneMap) {
        baseline_headroom = agtm.fHeadroomAdaptiveToneMap->fBaselineHdrHeadroom;
      }
    }

    if (baseline_headroom > 0.f) {
      // Use PQ to preserve values above SDR white.
      skcms_TransferFunction pq_trfn;
      skcms_TransferFunction_makePQ(&pq_trfn, hdr_reference_white);
      skcms_Matrix3x3 to_xyzd50;
      color_space->toXYZD50(&to_xyzd50);
      color_space = SkColorSpace::MakeRGB(pq_trfn, to_xyzd50);
    } else if (color_space->gammaIsLinear()) {
      // When encoding a linear float16 image, use an appropriate encoding to
      // not lose precision.
      color_space = color_space->makeSRGBGamma();
    }
  }

  return SkColorInfo(color_type, alpha_type, std::move(color_space));
}

std::optional<skhdr::Metadata> ImageEncoderUtils::ToSkHdrMetadata(
    const gfx::HDRMetadata& hdr_metadata) {
  if (hdr_metadata.IsEmpty()) {
    return std::nullopt;
  }
  skhdr::Metadata result;
  if (hdr_metadata.HasMDCV()) {
    result.setMasteringDisplayColorVolume(hdr_metadata.GetMDCV());
  }
  if (hdr_metadata.HasCLLI()) {
    result.setContentLightLevelInformation(hdr_metadata.GetCLLI());
  }
  if (hdr_metadata.HasAgtm()) {
    result.setAdaptiveGlobalToneMap(hdr_metadata.GetAgtm());
  }
  return result;
}

}  // namespace blink
