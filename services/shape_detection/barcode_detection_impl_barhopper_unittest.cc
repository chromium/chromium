// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/barcode_detection_provider_barhopper.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

namespace shape_detection {

constexpr struct TestParams {
  base::FilePath::StringPieceType filename;
  std::string_view expected_value;
  float x;
  float y;
  float width;
  float height;
} kTestParams[] = {
    {FILE_PATH_LITERAL("codabar.png"), "A6.2831853B", 24, 24, 448, 95},
    {FILE_PATH_LITERAL("code_39.png"), "CHROMIUM", 20, 20, 318, 75},
    {FILE_PATH_LITERAL("code_93.png"), "CHROMIUM", 20, 20, 216, 75},
    {FILE_PATH_LITERAL("code_128.png"), "Chromium", 20, 20, 246, 75},
    {FILE_PATH_LITERAL("data_matrix.png"), "Chromium", 11, 11, 53, 53},
    {FILE_PATH_LITERAL("ean_8.png"), "62831857", 14, 10, 134, 75},
    {FILE_PATH_LITERAL("ean_13.png"), "6283185307179", 27, 10, 190, 75},
    {FILE_PATH_LITERAL("itf.png"), "62831853071795", 10, 10, 135, 39},
    {FILE_PATH_LITERAL("pdf417.png"), "Chromium", 20, 20, 240, 44},
    {FILE_PATH_LITERAL("qr_code.png"), "https://chromium.org", 40, 40, 250,
     250},
    {FILE_PATH_LITERAL("upc_a.png"), "628318530714", 23, 10, 190, 75},
    {FILE_PATH_LITERAL("upc_e.png"), "06283186", 23, 10, 102, 75}};

class BarcodeDetectionImplBarhopperTest
    : public testing::TestWithParam<struct TestParams> {
 protected:
  BarcodeDetectionImplBarhopperTest() = default;
  BarcodeDetectionImplBarhopperTest(const BarcodeDetectionImplBarhopperTest&) =
      delete;
  BarcodeDetectionImplBarhopperTest& operator=(
      const BarcodeDetectionImplBarhopperTest&) = delete;
  ~BarcodeDetectionImplBarhopperTest() override = default;

  mojo::Remote<mojom::BarcodeDetection> ConnectToBarcodeDetector() {
    mojo::Remote<mojom::BarcodeDetectionProvider> provider;
    mojo::Remote<mojom::BarcodeDetection> barcode_service;

    BarcodeDetectionProviderBarhopper::Create(
        provider.BindNewPipeAndPassReceiver());

    auto options = mojom::BarcodeDetectorOptions::New();
    provider->CreateBarcodeDetection(
        barcode_service.BindNewPipeAndPassReceiver(), std::move(options));
    return barcode_service;
  }

  std::unique_ptr<SkBitmap> LoadTestImage(
      base::FilePath::StringPieceType filename) {
    // Load image data from test directory.
    base::FilePath image_path;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &image_path));
    image_path = image_path.Append(FILE_PATH_LITERAL("services"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(filename);
    EXPECT_TRUE(base::PathExists(image_path));
    std::string image_data;
    EXPECT_TRUE(base::ReadFileToString(image_path, &image_data));

    std::unique_ptr<SkBitmap> image(new SkBitmap());
    EXPECT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const uint8_t*>(image_data.data()), image_data.size(),
        image.get()));

    const gfx::Size size(image->width(), image->height());
    const uint32_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
    EXPECT_EQ(num_bytes, image->computeByteSize());

    return image;
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(BarcodeDetectionImplBarhopperTest, Scan) {
  mojo::Remote<mojom::BarcodeDetection> barcode_detector =
      ConnectToBarcodeDetector();

  std::unique_ptr<SkBitmap> image = LoadTestImage(GetParam().filename);
  ASSERT_TRUE(image);

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  base::RunLoop run_loop;
  barcode_detector->Detect(
      *image, base::BindLambdaForTesting(
                  [&](std::vector<mojom::BarcodeDetectionResultPtr> barcodes) {
                    results = std::move(barcodes);
                    run_loop.Quit();
                  }));
  run_loop.Run();
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(GetParam().expected_value, results.front()->raw_value);
  EXPECT_EQ(GetParam().x, results.front()->bounding_box.x());
  EXPECT_EQ(GetParam().y, results.front()->bounding_box.y());
  EXPECT_EQ(GetParam().width, results.front()->bounding_box.width());
  EXPECT_EQ(GetParam().height, results.front()->bounding_box.height());
}

INSTANTIATE_TEST_SUITE_P(,
                         BarcodeDetectionImplBarhopperTest,
                         testing::ValuesIn(kTestParams));

}  // namespace shape_detection
