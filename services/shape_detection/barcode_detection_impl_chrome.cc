// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_chrome.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "services/shape_detection/features.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom-shared.h"
#include "services/shape_detection/shape_detection_library_holder.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace shape_detection {

namespace {

gfx::RectF CornerPointsToBoundingBox(
    base::span<const ChromePointF>& corner_points) {
  float xmin = std::numeric_limits<float>::infinity();
  float ymin = std::numeric_limits<float>::infinity();
  float xmax = -std::numeric_limits<float>::infinity();
  float ymax = -std::numeric_limits<float>::infinity();
  for (auto& point : corner_points) {
    xmin = std::min(xmin, point.x);
    ymin = std::min(ymin, point.y);
    xmax = std::max(xmax, point.x);
    ymax = std::max(ymax, point.y);
  }
  return gfx::RectF(xmin, ymin, (xmax - xmin), (ymax - ymin));
}

mojom::BarcodeFormat BarcodeFormatToMojo(ChromeBarcodeFormat format) {
  switch (format) {
    case CHROME_BARCODE_FORMAT_UNKNOWN:
      return mojom::BarcodeFormat::UNKNOWN;
    case CHROME_BARCODE_FORMAT_AZTEC:
      return mojom::BarcodeFormat::AZTEC;
    case CHROME_BARCODE_FORMAT_CODE_128:
      return mojom::BarcodeFormat::CODE_128;
    case CHROME_BARCODE_FORMAT_CODE_39:
      return mojom::BarcodeFormat::CODE_39;
    case CHROME_BARCODE_FORMAT_CODE_93:
      return mojom::BarcodeFormat::CODE_93;
    case CHROME_BARCODE_FORMAT_CODABAR:
      return mojom::BarcodeFormat::CODABAR;
    case CHROME_BARCODE_FORMAT_DATA_MATRIX:
      return mojom::BarcodeFormat::DATA_MATRIX;
    case CHROME_BARCODE_FORMAT_EAN_13:
      return mojom::BarcodeFormat::EAN_13;
    case CHROME_BARCODE_FORMAT_EAN_8:
      return mojom::BarcodeFormat::EAN_8;
    case CHROME_BARCODE_FORMAT_ITF:
      return mojom::BarcodeFormat::ITF;
    case CHROME_BARCODE_FORMAT_PDF417:
      return mojom::BarcodeFormat::PDF417;
    case CHROME_BARCODE_FORMAT_QR_CODE:
      return mojom::BarcodeFormat::QR_CODE;
    case CHROME_BARCODE_FORMAT_UPC_A:
      return mojom::BarcodeFormat::UPC_A;
    case CHROME_BARCODE_FORMAT_UPC_E:
      return mojom::BarcodeFormat::UPC_E;
    default:
      NOTREACHED() << "Invalid barcode format";
  }
}

ChromeBarcodeFormat GetExpectedFormats(
    const shape_detection::mojom::BarcodeDetectorOptionsPtr& options) {
  ChromeBarcodeFormat expected_formats = CHROME_BARCODE_FORMAT_UNKNOWN;
  if (options->formats.empty()) {
    expected_formats =
        CHROME_BARCODE_FORMAT_AZTEC | CHROME_BARCODE_FORMAT_CODE_128 |
        CHROME_BARCODE_FORMAT_CODE_39 | CHROME_BARCODE_FORMAT_CODE_93 |
        CHROME_BARCODE_FORMAT_CODABAR | CHROME_BARCODE_FORMAT_DATA_MATRIX |
        CHROME_BARCODE_FORMAT_EAN_13 | CHROME_BARCODE_FORMAT_EAN_8 |
        CHROME_BARCODE_FORMAT_ITF | CHROME_BARCODE_FORMAT_PDF417 |
        CHROME_BARCODE_FORMAT_QR_CODE | CHROME_BARCODE_FORMAT_UPC_A |
        CHROME_BARCODE_FORMAT_UPC_E;
    return expected_formats;
  }

  for (const auto& format : options->formats) {
    switch (format) {
      case mojom::BarcodeFormat::AZTEC:
        expected_formats |= CHROME_BARCODE_FORMAT_AZTEC;
        break;
      case mojom::BarcodeFormat::CODE_128:
        expected_formats |= CHROME_BARCODE_FORMAT_CODE_128;
        break;
      case mojom::BarcodeFormat::CODE_39:
        expected_formats |= CHROME_BARCODE_FORMAT_CODE_39;
        break;
      case mojom::BarcodeFormat::CODE_93:
        expected_formats |= CHROME_BARCODE_FORMAT_CODE_93;
        break;
      case mojom::BarcodeFormat::CODABAR:
        expected_formats |= CHROME_BARCODE_FORMAT_CODABAR;
        break;
      case mojom::BarcodeFormat::DATA_MATRIX:
        expected_formats |= CHROME_BARCODE_FORMAT_DATA_MATRIX;
        break;
      case mojom::BarcodeFormat::EAN_13:
        expected_formats |= CHROME_BARCODE_FORMAT_EAN_13;
        break;
      case mojom::BarcodeFormat::EAN_8:
        expected_formats |= CHROME_BARCODE_FORMAT_EAN_8;
        break;
      case mojom::BarcodeFormat::ITF:
        expected_formats |= CHROME_BARCODE_FORMAT_ITF;
        break;
      case mojom::BarcodeFormat::PDF417:
        expected_formats |= CHROME_BARCODE_FORMAT_PDF417;
        break;
      case mojom::BarcodeFormat::QR_CODE:
        expected_formats |= CHROME_BARCODE_FORMAT_QR_CODE;
        break;
      case mojom::BarcodeFormat::UPC_E:
        expected_formats |= CHROME_BARCODE_FORMAT_UPC_E;
        break;
      case mojom::BarcodeFormat::UPC_A:
        expected_formats |= CHROME_BARCODE_FORMAT_UPC_A;
        break;
      case mojom::BarcodeFormat::UNKNOWN:
        expected_formats |= CHROME_BARCODE_FORMAT_UNKNOWN;
        break;
    }
  }

  return expected_formats;
}

}  // namespace

BarcodeDetectionImplChrome::BarcodeDetectionImplChrome(
    mojom::BarcodeDetectorOptionsPtr options)
    : expected_formats_(GetExpectedFormats(options)) {}

BarcodeDetectionImplChrome::~BarcodeDetectionImplChrome() = default;

DISABLE_CFI_DLSYM
void BarcodeDetectionImplChrome::Detect(
    const SkBitmap& bitmap,
    shape_detection::mojom::BarcodeDetection::DetectCallback callback) {
  int width = bitmap.width();
  int height = bitmap.height();
  std::vector<uint8_t> luminances(
      (base::CheckedNumeric<size_t>(width) * height).ValueOrDie());
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      SkColor color = bitmap.getColor(x, y);
      // Fast and approximate luminance calculation: (2*R + 5*G + B) / 8
      uint32_t luminance =
          2 * SkColorGetR(color) + 5 * SkColorGetG(color) + SkColorGetB(color);
      luminances[y * width + x] = luminance / 8;
    }
  }

  ChromeBarcodeDetectionResult* detection_results;
  size_t num_results;
  const auto* holder = ShapeDetectionLibraryHolder::GetInstance();
  // Holder should be valid if it passed pre-sandbox initialization.
  CHECK(holder);
  holder->api().DetectBarcodesWithFallback(
      width, height, luminances.data(), expected_formats_,
      base::FeatureList::IsEnabled(
          features::kBarhopperAztecRefineTransformFallback),
      &detection_results, &num_results);

  // SAFTY: `detection_results` was allocated with `num_results` by
  // ChromeShapeDetectionAPI.
  UNSAFE_BUFFERS(
      base::span<ChromeBarcodeDetectionResult> detection_results_span(
          detection_results, num_results);)
  std::vector<mojom::BarcodeDetectionResultPtr> results;
  for (const auto& barcode : detection_results_span) {
    auto result = shape_detection::mojom::BarcodeDetectionResult::New();

    // SAFTY: `barcode.corner_points` was allocated with `barcode.
    // corner_points_size` by ChromeShapeDetectionAPI.
    UNSAFE_BUFFERS(base::span<const ChromePointF> corner_points_span(
        barcode.corner_points, barcode.corner_points_size));
    result->bounding_box = CornerPointsToBoundingBox(corner_points_span);
    for (auto& corner_point : corner_points_span) {
      result->corner_points.emplace_back(corner_point.x, corner_point.y);
    }
    result->raw_value = std::string(
        reinterpret_cast<const char*>(barcode.value), barcode.value_size);
    result->format = BarcodeFormatToMojo(barcode.format);
    results.push_back(std::move(result));
  }

  holder->api().DestroyDetectionResults(detection_results, num_results);
  std::move(callback).Run(std::move(results));
}

// static
std::vector<mojom::BarcodeFormat>
BarcodeDetectionImplChrome::GetSupportedFormats() {
  return {mojom::BarcodeFormat::AZTEC,   mojom::BarcodeFormat::CODE_128,
          mojom::BarcodeFormat::CODE_39, mojom::BarcodeFormat::CODE_93,
          mojom::BarcodeFormat::CODABAR, mojom::BarcodeFormat::DATA_MATRIX,
          mojom::BarcodeFormat::EAN_13,  mojom::BarcodeFormat::EAN_8,
          mojom::BarcodeFormat::ITF,     mojom::BarcodeFormat::PDF417,
          mojom::BarcodeFormat::QR_CODE, mojom::BarcodeFormat::UPC_A,
          mojom::BarcodeFormat::UPC_E};
}

}  // namespace shape_detection
