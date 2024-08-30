// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_barhopper.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom-shared.h"
#include "third_party/barhopper/barhopper/barcode.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace shape_detection {

namespace {

gfx::RectF CornerPointsToBoundingBox(
    std::vector<barhopper::Point>& corner_points) {
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

mojom::BarcodeFormat BarhopperFormatToMojo(barhopper::BarcodeFormat format) {
  switch (format) {
    case barhopper::BarcodeFormat::AZTEC:
      return mojom::BarcodeFormat::AZTEC;
    case barhopper::BarcodeFormat::CODE_128:
      return mojom::BarcodeFormat::CODE_128;
    case barhopper::BarcodeFormat::CODE_39:
      return mojom::BarcodeFormat::CODE_39;
    case barhopper::BarcodeFormat::CODE_93:
      return mojom::BarcodeFormat::CODE_93;
    case barhopper::BarcodeFormat::CODABAR:
      return mojom::BarcodeFormat::CODABAR;
    case barhopper::BarcodeFormat::DATA_MATRIX:
      return mojom::BarcodeFormat::DATA_MATRIX;
    case barhopper::BarcodeFormat::EAN_13:
      return mojom::BarcodeFormat::EAN_13;
    case barhopper::BarcodeFormat::EAN_8:
      return mojom::BarcodeFormat::EAN_8;
    case barhopper::BarcodeFormat::ITF:
      return mojom::BarcodeFormat::ITF;
    case barhopper::BarcodeFormat::PDF417:
      return mojom::BarcodeFormat::PDF417;
    case barhopper::BarcodeFormat::QR_CODE:
      return mojom::BarcodeFormat::QR_CODE;
    case barhopper::BarcodeFormat::UPC_A:
      return mojom::BarcodeFormat::UPC_A;
    case barhopper::BarcodeFormat::UPC_E:
      return mojom::BarcodeFormat::UPC_E;
    case barhopper::BarcodeFormat::UNRECOGNIZED:
      return mojom::BarcodeFormat::UNKNOWN;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid barcode format";
      return mojom::BarcodeFormat::UNKNOWN;
  }
}

barhopper::RecognitionOptions GetRecognitionOptions(
    const shape_detection::mojom::BarcodeDetectorOptionsPtr& options) {
  barhopper::RecognitionOptions recognition_options;
  if (options->formats.empty()) {
    recognition_options.barcode_formats =
        barhopper::BarcodeFormat::AZTEC | barhopper::BarcodeFormat::CODE_128 |
        barhopper::BarcodeFormat::CODE_39 | barhopper::BarcodeFormat::CODE_93 |
        barhopper::BarcodeFormat::CODABAR |
        barhopper::BarcodeFormat::DATA_MATRIX |
        barhopper::BarcodeFormat::EAN_13 | barhopper::BarcodeFormat::EAN_8 |
        barhopper::BarcodeFormat::ITF | barhopper::BarcodeFormat::PDF417 |
        barhopper::BarcodeFormat::QR_CODE | barhopper::BarcodeFormat::UPC_A |
        barhopper::BarcodeFormat::UPC_E;
    return recognition_options;
  }

  int recognition_formats = 0;
  for (const auto& format : options->formats) {
    switch (format) {
      case mojom::BarcodeFormat::AZTEC:
        recognition_formats |= barhopper::BarcodeFormat::AZTEC;
        break;
      case mojom::BarcodeFormat::CODE_128:
        recognition_formats |= barhopper::BarcodeFormat::CODE_128;
        break;
      case mojom::BarcodeFormat::CODE_39:
        recognition_formats |= barhopper::BarcodeFormat::CODE_39;
        break;
      case mojom::BarcodeFormat::CODE_93:
        recognition_formats |= barhopper::BarcodeFormat::CODE_93;
        break;
      case mojom::BarcodeFormat::CODABAR:
        recognition_formats |= barhopper::BarcodeFormat::CODABAR;
        break;
      case mojom::BarcodeFormat::DATA_MATRIX:
        recognition_formats |= barhopper::BarcodeFormat::DATA_MATRIX;
        break;
      case mojom::BarcodeFormat::EAN_13:
        recognition_formats |= barhopper::BarcodeFormat::EAN_13;
        break;
      case mojom::BarcodeFormat::EAN_8:
        recognition_formats |= barhopper::BarcodeFormat::EAN_8;
        break;
      case mojom::BarcodeFormat::ITF:
        recognition_formats |= barhopper::BarcodeFormat::ITF;
        break;
      case mojom::BarcodeFormat::PDF417:
        recognition_formats |= barhopper::BarcodeFormat::PDF417;
        break;
      case mojom::BarcodeFormat::QR_CODE:
        recognition_formats |= barhopper::BarcodeFormat::QR_CODE;
        break;
      case mojom::BarcodeFormat::UPC_E:
        recognition_formats |= barhopper::BarcodeFormat::UPC_E;
        break;
      case mojom::BarcodeFormat::UPC_A:
        recognition_formats |= barhopper::BarcodeFormat::UPC_A;
        break;
      case mojom::BarcodeFormat::UNKNOWN:
        recognition_formats |= barhopper::BarcodeFormat::UNRECOGNIZED;
        break;
    }
  }
  recognition_options.barcode_formats = recognition_formats;
  return recognition_options;
}

}  // namespace

BarcodeDetectionImplBarhopper::BarcodeDetectionImplBarhopper(
    mojom::BarcodeDetectorOptionsPtr options)
    : recognition_options_(GetRecognitionOptions(options)) {}

BarcodeDetectionImplBarhopper::~BarcodeDetectionImplBarhopper() = default;

void BarcodeDetectionImplBarhopper::Detect(
    const SkBitmap& bitmap,
    shape_detection::mojom::BarcodeDetection::DetectCallback callback) {
  int width = bitmap.width();
  int height = bitmap.height();
  std::vector<uint8_t> luminances(height * width);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      SkColor color = bitmap.getColor(x, y);
      // Fast and approximate luminance calculation: (2*R + 5*G + B) / 8
      uint32_t luminance =
          2 * SkColorGetR(color) + 5 * SkColorGetG(color) + SkColorGetB(color);
      luminances[y * width + x] = luminance / 8;
    }
  }
  std::vector<barhopper::Barcode> barcodes;
  barhopper::Barhopper::Recognize(width, height, luminances.data(),
                                  recognition_options_, &barcodes);

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  for (auto& barcode : barcodes) {
    auto result = shape_detection::mojom::BarcodeDetectionResult::New();
    result->bounding_box = CornerPointsToBoundingBox(barcode.corner_point);
    for (auto& corner_point : barcode.corner_point) {
      result->corner_points.emplace_back(corner_point.x, corner_point.y);
    }
    result->raw_value = barcode.raw_value;
    result->format = BarhopperFormatToMojo(barcode.format);
    results.push_back(std::move(result));
  }
  std::move(callback).Run(std::move(results));
}

// static
std::vector<mojom::BarcodeFormat>
BarcodeDetectionImplBarhopper::GetSupportedFormats() {
  return {mojom::BarcodeFormat::AZTEC,   mojom::BarcodeFormat::CODE_128,
          mojom::BarcodeFormat::CODE_39, mojom::BarcodeFormat::CODE_93,
          mojom::BarcodeFormat::CODABAR, mojom::BarcodeFormat::DATA_MATRIX,
          mojom::BarcodeFormat::EAN_13,  mojom::BarcodeFormat::EAN_8,
          mojom::BarcodeFormat::ITF,     mojom::BarcodeFormat::PDF417,
          mojom::BarcodeFormat::QR_CODE, mojom::BarcodeFormat::UPC_A,
          mojom::BarcodeFormat::UPC_E};
}

}  // namespace shape_detection
