// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/barcode_detection_provider_chrome.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

namespace shape_detection {

constexpr size_t kMaxNumExpectedValues = 2;

struct ExpectedValue {
  std::string_view raw_value;
  float x;
  float y;
  float width;
  float height;
};

constexpr struct TestParams {
  base::FilePath::StringViewType filename;
  size_t num_expected_values;
  std::array<ExpectedValue, kMaxNumExpectedValues> expected_value;
} kTestParams[] = {
    {FILE_PATH_LITERAL("codabar.png"),
     1u,
     {{{"A6.2831853B", 24, 24, 448, 95}}}},
    {FILE_PATH_LITERAL("code_39.png"), 1u, {{{"CHROMIUM", 20, 20, 318, 75}}}},
    {FILE_PATH_LITERAL("code_93.png"), 1u, {{{"CHROMIUM", 20, 20, 216, 75}}}},
    {FILE_PATH_LITERAL("code_128.png"), 1u, {{{"Chromium", 20, 20, 246, 75}}}},
    {FILE_PATH_LITERAL("data_matrix.png"),
     1u,
     {{{"Chromium", 11, 11, 53, 53}}}},
    {FILE_PATH_LITERAL("ean_8.png"), 1u, {{{"62831857", 14, 10, 134, 75}}}},
    {FILE_PATH_LITERAL("ean_13.png"),
     1u,
     {{{"6283185307179", 27, 10, 190, 75}}}},
    {FILE_PATH_LITERAL("itf.png"), 1u, {{{"62831853071795", 10, 10, 135, 39}}}},
    {FILE_PATH_LITERAL("pdf417.png"), 1u, {{{"Chromium", 20, 20, 240, 44}}}},
    {FILE_PATH_LITERAL("qr_code.png"),
     1u,
     {{{"https://chromium.org", 40, 40, 250, 250}}}},
    {FILE_PATH_LITERAL("upc_a.png"), 1u, {{{"628318530714", 23, 10, 190, 75}}}},
    {FILE_PATH_LITERAL("upc_e.png"), 1u, {{{"06283186", 23, 10, 102, 75}}}},
    {FILE_PATH_LITERAL("two_upc_a.png"),
     2u,
     {{
         {"326565565892", 191, 265, 358, 174},
         {"565656545454", 731, 260, 357, 5},
     }}},
};

class BarcodeDetectionImplChromeTest
    : public testing::TestWithParam<struct TestParams> {
 protected:
  BarcodeDetectionImplChromeTest() = default;
  BarcodeDetectionImplChromeTest(const BarcodeDetectionImplChromeTest&) =
      delete;
  BarcodeDetectionImplChromeTest& operator=(
      const BarcodeDetectionImplChromeTest&) = delete;
  ~BarcodeDetectionImplChromeTest() override = default;

  mojo::Remote<mojom::BarcodeDetection> ConnectToBarcodeDetector() {
    mojo::Remote<mojom::BarcodeDetectionProvider> provider;
    mojo::Remote<mojom::BarcodeDetection> barcode_service;

    BarcodeDetectionProviderChrome::Create(
        provider.BindNewPipeAndPassReceiver());

    auto options = mojom::BarcodeDetectorOptions::New();
    provider->CreateBarcodeDetection(
        barcode_service.BindNewPipeAndPassReceiver(), std::move(options));
    return barcode_service;
  }

  SkBitmap LoadTestImage(base::FilePath::StringViewType filename) {
    // Load image data from test directory.
    base::FilePath image_path;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &image_path));
    image_path = image_path.Append(FILE_PATH_LITERAL("services"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(filename);
    EXPECT_TRUE(base::PathExists(image_path));
    std::optional<std::vector<uint8_t>> image_data =
        base::ReadFileToBytes(image_path);

    SkBitmap image = gfx::PNGCodec::Decode(image_data.value());
    EXPECT_FALSE(image.isNull());

    const gfx::Size size(image.width(), image.height());
    const uint32_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
    EXPECT_EQ(num_bytes, image.computeByteSize());

    return image;
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(BarcodeDetectionImplChromeTest, Scan) {
  mojo::Remote<mojom::BarcodeDetection> barcode_detector =
      ConnectToBarcodeDetector();

  SkBitmap image = LoadTestImage(GetParam().filename);

  std::vector<mojom::BarcodeDetectionResultPtr> results;
  base::RunLoop run_loop;
  barcode_detector->Detect(
      image, base::BindLambdaForTesting(
                 [&](std::vector<mojom::BarcodeDetectionResultPtr> barcodes) {
                   results = std::move(barcodes);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  EXPECT_EQ(GetParam().num_expected_values, results.size());
  ASSERT_LE(results.size(), kMaxNumExpectedValues);
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(GetParam().expected_value[i].raw_value, results[i]->raw_value);
    EXPECT_EQ(GetParam().expected_value[i].x, results[i]->bounding_box.x());
    EXPECT_EQ(GetParam().expected_value[i].y, results[i]->bounding_box.y());
    EXPECT_EQ(GetParam().expected_value[i].width,
              results[i]->bounding_box.width());
    EXPECT_EQ(GetParam().expected_value[i].height,
              results[i]->bounding_box.height());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         BarcodeDetectionImplChromeTest,
                         testing::ValuesIn(kTestParams));

}  // namespace shape_detection
