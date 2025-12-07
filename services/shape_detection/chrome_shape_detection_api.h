// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_CHROME_SHAPE_DETECTION_API_H_
#define SERVICES_SHAPE_DETECTION_CHROME_SHAPE_DETECTION_API_H_

#include <cstddef>
#include <cstdint>

// This header defines the public interface to the Chrome Shape Detection shared
// library.

extern "C" {

#define CHROME_BARCODE_FORMAT_UNKNOWN (0)
#define CHROME_BARCODE_FORMAT_AZTEC (1 << 0)
#define CHROME_BARCODE_FORMAT_CODE_128 (1 << 1)
#define CHROME_BARCODE_FORMAT_CODE_39 (1 << 2)
#define CHROME_BARCODE_FORMAT_CODE_93 (1 << 3)
#define CHROME_BARCODE_FORMAT_CODABAR (1 << 4)
#define CHROME_BARCODE_FORMAT_DATA_MATRIX (1 << 5)
#define CHROME_BARCODE_FORMAT_EAN_13 (1 << 6)
#define CHROME_BARCODE_FORMAT_EAN_8 (1 << 7)
#define CHROME_BARCODE_FORMAT_ITF (1 << 8)
#define CHROME_BARCODE_FORMAT_PDF417 (1 << 9)
#define CHROME_BARCODE_FORMAT_QR_CODE (1 << 10)
#define CHROME_BARCODE_FORMAT_UPC_A (1 << 11)
#define CHROME_BARCODE_FORMAT_UPC_E (1 << 12)

// Bitmap of expected barcode formats.
using ChromeBarcodeFormat = uint32_t;

struct ChromePointF {
  float x;
  float y;
};

struct ChromeBarcodeDetectionResult {
  uint8_t* value;
  size_t value_size;
  ChromePointF* corner_points;
  size_t corner_points_size;
  ChromeBarcodeFormat format;
};

// IMPORTANT: All functions that call ChromeShapeDetectionAPI should be
// annotated with DISABLE_CFI_DLSYM.

// Table of C API functions defined within the library.
struct ChromeShapeDetectionAPI {
  // Detects barcodes in the given grayscale image.
  void (*DetectBarcodes)(size_t width,
                         size_t height,
                         uint8_t* data,
                         ChromeBarcodeFormat expected_formats,
                         ChromeBarcodeDetectionResult** results,
                         size_t* results_size);

  void (*DestroyDetectionResults)(ChromeBarcodeDetectionResult* results,
                                  size_t size);

  // TODO(crbug.com/442001297): Remove once the fallback is default-enabled.
  void (*DetectBarcodesWithFallback)(
      size_t width,
      size_t height,
      uint8_t* data,
      ChromeBarcodeFormat expected_formats,
      bool enable_noop_fallback_in_refine_transform_failure,
      ChromeBarcodeDetectionResult** results,
      size_t* results_size);
};

// Signature of the GetChromeShapeDetectionAPI() function which the shared
// library exports.
using ChromeShapeDetectionAPIGetter = const ChromeShapeDetectionAPI* (*)();

}  // extern "C"

#endif  // SERVICES_SHAPE_DETECTION_CHROME_SHAPE_DETECTION_API_H_
