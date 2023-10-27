// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac_vision.h"

#import <Vision/Vision.h>

#include <memory>
#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gl/gl_switches.h"

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacVision(
    mojom::BarcodeDetectorOptionsPtr options) {
  return std::make_unique<BarcodeDetectionImplMacVision>(std::move(options));
}

using BarcodeDetectorFactory =
    base::RepeatingCallback<std::unique_ptr<mojom::BarcodeDetection>(
        mojom::BarcodeDetectorOptionsPtr)>;

const std::string kInfoString = "https://www.chromium.org";

struct TestParams {
  bool allow_duplicates;
  mojom::BarcodeFormat symbology;
  BarcodeDetectorFactory factory;
  NSString* __strong test_code_generator;
} kTestParams[] = {
    // Vision only supports a number of 1D/2D codes. Not all of them are
    // available for generation, though, only a few.
    {false, mojom::BarcodeFormat::PDF417,
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIPDF417BarcodeGenerator"},
    {false, mojom::BarcodeFormat::QR_CODE,
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIQRCodeGenerator"},
    {true,  // 1D barcode makes the detector find the same code several times.
     mojom::BarcodeFormat::CODE_128,
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CICode128BarcodeGenerator"}};
}

class BarcodeDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionImplMacTest() override = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_P(BarcodeDetectionImplMacTest, CreateAndDestroy) {
  std::unique_ptr<mojom::BarcodeDetection> impl =
      GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }
}

// This test generates a single barcode and scans it back.
TEST_P(BarcodeDetectionImplMacTest, ScanOneBarcode) {
  // Barcode detection needs GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  std::unique_ptr<mojom::BarcodeDetection> impl =
      GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());

  // Generate a barcode image as a CIImage by using |qr_code_generator|.
  NSData* const qr_code_data = [base::SysUTF8ToNSString(kInfoString)
      dataUsingEncoding:NSISOLatin1StringEncoding];

  CIFilter* qr_code_generator =
      [CIFilter filterWithName:GetParam().test_code_generator];
  [qr_code_generator setValue:qr_code_data forKey:@"inputMessage"];

  CIImage* qr_code_image = qr_code_generator.outputImage;

  const gfx::Size size(qr_code_image.extent.size.width,
                       qr_code_image.extent.size.height);

  CIContext* context = [[CIContext alloc] init];

  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      [context createCGImage:qr_code_image fromRect:qr_code_image.extent]);
  EXPECT_EQ(static_cast<size_t>(size.width()), CGImageGetWidth(cg_image.get()));
  EXPECT_EQ(static_cast<size_t>(size.height()),
            CGImageGetHeight(cg_image.get()));

  SkBitmap bitmap;
  ASSERT_TRUE(SkCreateBitmapFromCGImage(&bitmap, cg_image.get()));

  base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>> future;
  impl->Detect(bitmap, future.GetCallback());

  auto results = future.Take();
  if (GetParam().allow_duplicates) {
    EXPECT_GE(results.size(), 1u);
  } else {
    EXPECT_EQ(results.size(), 1u);
  }
  for (const auto& barcode : results) {
    EXPECT_EQ(kInfoString, barcode->raw_value);
    EXPECT_EQ(GetParam().symbology, barcode->format);
  }
}

INSTANTIATE_TEST_SUITE_P(, BarcodeDetectionImplMacTest, ValuesIn(kTestParams));

TEST_F(BarcodeDetectionImplMacTest, HintFormats) {
  auto vision_impl = std::make_unique<BarcodeDetectionImplMacVision>(
      mojom::BarcodeDetectorOptions::New());
  EXPECT_EQ(vision_impl->GetSymbologyHintsForTesting().count, 0u);

  mojom::BarcodeDetectorOptionsPtr options =
      mojom::BarcodeDetectorOptions::New();
  options->formats = {
      mojom::BarcodeFormat::PDF417, mojom::BarcodeFormat::QR_CODE,
      mojom::BarcodeFormat::CODE_128, mojom::BarcodeFormat::ITF};
  vision_impl =
      std::make_unique<BarcodeDetectionImplMacVision>(std::move(options));
  NSArray<VNBarcodeSymbology>* expected = @[
    VNBarcodeSymbologyPDF417, VNBarcodeSymbologyQR, VNBarcodeSymbologyCode128,
    VNBarcodeSymbologyITF14, VNBarcodeSymbologyI2of5,
    VNBarcodeSymbologyI2of5Checksum
  ];
  EXPECT_TRUE([vision_impl->GetSymbologyHintsForTesting() isEqualTo:expected]);
}

}  // shape_detection namespace
