// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/barcode_detection_impl_mac.h"

#include <dlfcn.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/shape_detection/barcode_detection_impl_mac_vision.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gl/gl_switches.h"

using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace shape_detection {

namespace {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacCoreImage(
    mojom::BarcodeDetectorOptionsPtr options) {
  return std::make_unique<BarcodeDetectionImplMac>();
}

std::unique_ptr<mojom::BarcodeDetection> CreateBarcodeDetectorImplMacVision(
    mojom::BarcodeDetectorOptionsPtr options) {
  if (@available(macOS 10.13, *)) {
    if (!BarcodeDetectionImplMacVision::IsBlockedMacOSVersion()) {
      return std::make_unique<BarcodeDetectionImplMacVision>(
          std::move(options));
    }
  }
  return nullptr;
}

void* LoadVisionLibrary() {
  if (@available(macOS 10.13, *)) {
    return dlopen("/System/Library/Frameworks/Vision.framework/Vision",
                  RTLD_LAZY);
  }
  return nullptr;
}

using LibraryLoadCB = base::RepeatingCallback<void*(void)>;

using BarcodeDetectorFactory =
    base::RepeatingCallback<std::unique_ptr<mojom::BarcodeDetection>(
        mojom::BarcodeDetectorOptionsPtr)>;

const std::string kInfoString = "https://www.chromium.org";

struct TestParams {
  size_t num_barcodes;
  mojom::BarcodeFormat symbology;
  LibraryLoadCB library_load_callback;
  BarcodeDetectorFactory factory;
  NSString* test_code_generator;
} kTestParams[] = {
    // CoreImage only supports QR Codes.
    {1, mojom::BarcodeFormat::QR_CODE,
     base::BindRepeating([]() { return static_cast<void*>(nullptr); }),
     base::BindRepeating(&CreateBarcodeDetectorImplMacCoreImage),
     @"CIQRCodeGenerator"},
    // Vision only supports a number of 1D/2D codes. Not all of them are
    // available for generation, though, only a few.
    {1, mojom::BarcodeFormat::PDF417, base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIPDF417BarcodeGenerator"},
    {1, mojom::BarcodeFormat::QR_CODE, base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CIQRCodeGenerator"},
    {6 /* 1D barcode makes the detector find the same code several times. */,
     mojom::BarcodeFormat::CODE_128, base::BindRepeating(&LoadVisionLibrary),
     base::BindRepeating(&CreateBarcodeDetectorImplMacVision),
     @"CICode128BarcodeGenerator"}};
}

class BarcodeDetectionImplMacTest : public TestWithParam<struct TestParams> {
 public:
  ~BarcodeDetectionImplMacTest() override = default;

  void SetUp() override {}

  void TearDown() override {
    if (vision_framework_)
      dlclose(vision_framework_);
  }

  void DetectCallback(size_t num_barcodes,
                      mojom::BarcodeFormat symbology,
                      const std::string& barcode_value,
                      std::vector<mojom::BarcodeDetectionResultPtr> results) {
    EXPECT_EQ(num_barcodes, results.size());
    for (const auto& barcode : results) {
      EXPECT_EQ(barcode_value, barcode->raw_value);
      EXPECT_EQ(symbology, barcode->format);
    }

    Detection();
  }
  MOCK_METHOD0(Detection, void(void));

  std::unique_ptr<mojom::BarcodeDetection> impl_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  void* vision_framework_ = nullptr;
};

TEST_P(BarcodeDetectionImplMacTest, CreateAndDestroy) {
  impl_ = GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl_) {
    LOG(WARNING) << "Barcode Detection for this (library, OS version) pair is "
                    "not supported, skipping test.";
    return;
  }
}

// This test generates a single barcode and scans it back.
TEST_P(BarcodeDetectionImplMacTest, ScanOneBarcode) {
  // Barcode detection needs at least MAC OS X 10.10, and GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  impl_ = GetParam().factory.Run(mojom::BarcodeDetectorOptions::New());
  if (!impl_) {
    LOG(WARNING) << "Barcode Detection is not supported before Mac OSX 10.10."
                 << "Skipping test.";
    return;
  }
  vision_framework_ = GetParam().library_load_callback.Run();

  // Generate a barcode image as a CIImage by using |qr_code_generator|.
  NSData* const qr_code_data =
      [[NSString stringWithUTF8String:kInfoString.c_str()]
          dataUsingEncoding:NSISOLatin1StringEncoding];

  CIFilter* qr_code_generator =
      [CIFilter filterWithName:GetParam().test_code_generator];
  [qr_code_generator setValue:qr_code_data forKey:@"inputMessage"];

  // [CIImage outputImage] is available in macOS 10.10+.  Could be added to
  // sdk_forward_declarations.h but seems hardly worth it.
  EXPECT_TRUE([qr_code_generator respondsToSelector:@selector(outputImage)]);
  CIImage* qr_code_image =
      [qr_code_generator performSelector:@selector(outputImage)];

  const gfx::Size size([qr_code_image extent].size.width,
                       [qr_code_image extent].size.height);

  base::scoped_nsobject<CIContext> context([[CIContext alloc] init]);

  base::ScopedCFTypeRef<CGImageRef> cg_image(
      [context createCGImage:qr_code_image fromRect:[qr_code_image extent]]);
  EXPECT_EQ(static_cast<size_t>(size.width()), CGImageGetWidth(cg_image));
  EXPECT_EQ(static_cast<size_t>(size.height()), CGImageGetHeight(cg_image));

  SkBitmap bitmap;
  ASSERT_TRUE(SkCreateBitmapFromCGImage(&bitmap, cg_image));

  base::RunLoop run_loop;
  // Send the image Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection()).WillOnce(RunClosure(run_loop.QuitClosure()));
  // TODO(crbug.com/938663): expect detected symbology.
  impl_->Detect(bitmap,
                base::BindOnce(&BarcodeDetectionImplMacTest::DetectCallback,
                               base::Unretained(this), GetParam().num_barcodes,
                               GetParam().symbology, kInfoString));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(, BarcodeDetectionImplMacTest, ValuesIn(kTestParams));

TEST_F(BarcodeDetectionImplMacTest, HintFormats) {
  if (@available(macOS 10.13, *)) {
    vision_framework_ = LoadVisionLibrary();

    auto vision_impl = std::make_unique<BarcodeDetectionImplMacVision>(
        mojom::BarcodeDetectorOptions::New());
    EXPECT_EQ([vision_impl->GetSymbologyHintsForTesting() count], 0u);

    mojom::BarcodeDetectorOptionsPtr options =
        mojom::BarcodeDetectorOptions::New();
    options->formats = {
        mojom::BarcodeFormat::PDF417, mojom::BarcodeFormat::QR_CODE,
        mojom::BarcodeFormat::CODE_128, mojom::BarcodeFormat::ITF};
    vision_impl =
        std::make_unique<BarcodeDetectionImplMacVision>(std::move(options));
    NSSet* expected = [NSSet
        setWithObjects:@"VNBarcodeSymbologyPDF417", @"VNBarcodeSymbologyQR",
                       @"VNBarcodeSymbologyCode128", @"VNBarcodeSymbologyITF14",
                       @"VNBarcodeSymbologyI2of5",
                       @"VNBarcodeSymbologyI2of5Checksum", nil];
    EXPECT_TRUE(
        [vision_impl->GetSymbologyHintsForTesting() isEqualTo:expected]);
  } else {
    LOG(WARNING) << "Barcode Detection with Vision not supported before 10.13, "
                 << "skipping test.";
  }
}

}  // shape_detection namespace
