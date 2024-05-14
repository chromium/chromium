// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision.h"

#import <Foundation/Foundation.h>
#import <Vision/Vision.h>

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shape_detection {

namespace {

mojom::BarcodeFormat ToBarcodeFormat(NSString* symbology) {
  if ([symbology isEqual:VNBarcodeSymbologyAztec])
    return mojom::BarcodeFormat::AZTEC;
  if ([symbology isEqual:VNBarcodeSymbologyCode128])
    return mojom::BarcodeFormat::CODE_128;
  if ([symbology isEqual:VNBarcodeSymbologyCode39] ||
      [symbology isEqual:VNBarcodeSymbologyCode39Checksum] ||
      [symbology isEqual:VNBarcodeSymbologyCode39FullASCII] ||
      [symbology isEqual:VNBarcodeSymbologyCode39FullASCIIChecksum]) {
    return mojom::BarcodeFormat::CODE_39;
  }
  if ([symbology isEqual:VNBarcodeSymbologyCode93] ||
      [symbology isEqual:VNBarcodeSymbologyCode93i]) {
    return mojom::BarcodeFormat::CODE_93;
  }
  if ([symbology isEqual:VNBarcodeSymbologyDataMatrix])
    return mojom::BarcodeFormat::DATA_MATRIX;
  if ([symbology isEqual:VNBarcodeSymbologyEAN13])
    return mojom::BarcodeFormat::EAN_13;
  if ([symbology isEqual:VNBarcodeSymbologyEAN8])
    return mojom::BarcodeFormat::EAN_8;
  if ([symbology isEqual:VNBarcodeSymbologyITF14] ||
      [symbology isEqual:VNBarcodeSymbologyI2of5] ||
      [symbology isEqual:VNBarcodeSymbologyI2of5Checksum]) {
    return mojom::BarcodeFormat::ITF;
  }
  if ([symbology isEqual:VNBarcodeSymbologyPDF417])
    return mojom::BarcodeFormat::PDF417;
  if ([symbology isEqual:VNBarcodeSymbologyQR])
    return mojom::BarcodeFormat::QR_CODE;
  if ([symbology isEqual:VNBarcodeSymbologyUPCE])
    return mojom::BarcodeFormat::UPC_E;
  return mojom::BarcodeFormat::UNKNOWN;
}

void UpdateSymbologyHint(mojom::BarcodeFormat format,
                         NSMutableArray<VNBarcodeSymbology>* hint) {
  switch (format) {
    case mojom::BarcodeFormat::AZTEC:
      [hint addObject:VNBarcodeSymbologyAztec];
      return;
    case mojom::BarcodeFormat::CODE_128:
      [hint addObject:VNBarcodeSymbologyCode128];
      return;
    case mojom::BarcodeFormat::CODE_39:
      [hint addObjectsFromArray:@[
        VNBarcodeSymbologyCode39, VNBarcodeSymbologyCode39Checksum,
        VNBarcodeSymbologyCode39FullASCII,
        VNBarcodeSymbologyCode39FullASCIIChecksum
      ]];
      return;
    case mojom::BarcodeFormat::CODE_93:
      [hint addObjectsFromArray:@[
        VNBarcodeSymbologyCode93, VNBarcodeSymbologyCode93i
      ]];
      return;
    case mojom::BarcodeFormat::CODABAR:
      return;
    case mojom::BarcodeFormat::DATA_MATRIX:
      [hint addObject:VNBarcodeSymbologyDataMatrix];
      return;
    case mojom::BarcodeFormat::EAN_13:
      [hint addObject:VNBarcodeSymbologyEAN13];
      return;
    case mojom::BarcodeFormat::EAN_8:
      [hint addObject:VNBarcodeSymbologyEAN8];
      return;
    case mojom::BarcodeFormat::ITF:
      [hint addObjectsFromArray:@[
        VNBarcodeSymbologyITF14, VNBarcodeSymbologyI2of5,
        VNBarcodeSymbologyI2of5Checksum
      ]];
      return;
    case mojom::BarcodeFormat::PDF417:
      [hint addObject:VNBarcodeSymbologyPDF417];
      return;
    case mojom::BarcodeFormat::QR_CODE:
      [hint addObject:VNBarcodeSymbologyQR];
      return;
    case mojom::BarcodeFormat::UPC_A:
      return;
    case mojom::BarcodeFormat::UPC_E:
      [hint addObject:VNBarcodeSymbologyUPCE];
      return;
    case mojom::BarcodeFormat::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

}  // namespace

BarcodeDetectionImplMacVision::BarcodeDetectionImplMacVision(
    mojom::BarcodeDetectorOptionsPtr options)
    : weak_factory_(this) {
  NSMutableArray<VNBarcodeSymbology>* symbology_hints = [NSMutableArray array];
  for (const auto& hint : options->formats) {
    if (hint == mojom::BarcodeFormat::UNKNOWN) {
      mojo::ReportBadMessage("Formats hint contains UNKNOWN BarcodeFormat.");
      return;
    }

    UpdateSymbologyHint(hint, symbology_hints);
  }
  symbology_hints_ = symbology_hints;

  // The repeating callback will not be run if BarcodeDetectionImplMacVision
  // object has already been destroyed.
  barcodes_async_request_ = VisionAPIAsyncRequestMac::Create(
      [VNDetectBarcodesRequest class],
      base::BindRepeating(&BarcodeDetectionImplMacVision::OnBarcodesDetected,
                          weak_factory_.GetWeakPtr()),
      symbology_hints_);
}

BarcodeDetectionImplMacVision::~BarcodeDetectionImplMacVision() = default;

void BarcodeDetectionImplMacVision::Detect(const SkBitmap& bitmap,
                                           DetectCallback callback) {
  DCHECK(barcodes_async_request_);

  if (!barcodes_async_request_->PerformRequest(bitmap)) {
    std::move(callback).Run({});
    return;
  }

  image_size_ = CGSizeMake(bitmap.width(), bitmap.height());
  // Hold on the callback until async request completes.
  detected_callback_ = std::move(callback);
  // This prevents the Detect function from being called before the
  // VisionAPIAsyncRequestMac completes.
  if (receiver_)  // Can be unbound in unit testing.
    receiver_->PauseIncomingMethodCallProcessing();
}

void BarcodeDetectionImplMacVision::OnBarcodesDetected(VNRequest* request,
                                                       NSError* error) {
  if (receiver_)  // Can be unbound in unit testing.
    receiver_->ResumeIncomingMethodCallProcessing();

  if ([request.results count] == 0 || error) {
    std::move(detected_callback_).Run({});
    return;
  }

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  for (VNBarcodeObservation* const observation in request.results) {
    auto barcode = mojom::BarcodeDetectionResult::New();
    // The coordinates are normalized to the dimensions of the processed image.
    barcode->bounding_box = ConvertCGToGfxCoordinates(
        CGRectMake(observation.boundingBox.origin.x * image_size_.width,
                   observation.boundingBox.origin.y * image_size_.height,
                   observation.boundingBox.size.width * image_size_.width,
                   observation.boundingBox.size.height * image_size_.height),
        image_size_.height);

    // Enumerate corner points starting from top-left in clockwise fashion:
    // https://wicg.github.io/shape-detection-api/#dom-detectedbarcode-cornerpoints
    barcode->corner_points.emplace_back(
        observation.topLeft.x * image_size_.width,
        (1 - observation.topLeft.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.topRight.x * image_size_.width,
        (1 - observation.topRight.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.bottomRight.x * image_size_.width,
        (1 - observation.bottomRight.y) * image_size_.height);
    barcode->corner_points.emplace_back(
        observation.bottomLeft.x * image_size_.width,
        (1 - observation.bottomLeft.y) * image_size_.height);

    barcode->raw_value =
        base::SysNSStringToUTF8(observation.payloadStringValue);

    barcode->format = ToBarcodeFormat(observation.symbology);

    results.push_back(std::move(barcode));
  }
  std::move(detected_callback_).Run(std::move(results));
}

// static
std::vector<shape_detection::mojom::BarcodeFormat>
BarcodeDetectionImplMacVision::GetSupportedSymbologies(
    VisionAPIInterface* vision_api) {
  std::unique_ptr<VisionAPIInterface> scoped_vision_api;
  if (!vision_api) {
    scoped_vision_api = VisionAPIInterface::Create();
    vision_api = scoped_vision_api.get();
  }
  base::flat_set<shape_detection::mojom::BarcodeFormat> results;
  NSArray<NSString*>* symbologies = vision_api->GetSupportedSymbologies();

  results.reserve(symbologies.count);
  for (VNBarcodeSymbology symbology : symbologies) {
    auto converted = ToBarcodeFormat(symbology);
    if (converted == shape_detection::mojom::BarcodeFormat::UNKNOWN) {
      DLOG(WARNING) << "Symbology " << base::SysNSStringToUTF8(symbology)
                    << " unknown to spec.";
      continue;
    }
    results.insert(converted);
  }
  return std::vector<shape_detection::mojom::BarcodeFormat>(results.begin(),
                                                            results.end());
}

NSArray<VNBarcodeSymbology>*
BarcodeDetectionImplMacVision::GetSymbologyHintsForTesting() {
  return symbology_hints_;
}

}  // namespace shape_detection
