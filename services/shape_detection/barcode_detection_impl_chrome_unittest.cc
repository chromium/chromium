// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/barcode_detection_provider_chrome.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
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
  base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>> future;
  barcode_detector->Detect(image, future.GetCallback());
  std::vector<mojom::BarcodeDetectionResultPtr> results = future.Take();
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

TEST_F(BarcodeDetectionImplChromeTest, RejectsTooSmallDimensions) {
  mojo::Remote<mojom::BarcodeDetection> barcode_detector =
      ConnectToBarcodeDetector();

  for (const gfx::Size& size :
       {gfx::Size(1, 100), gfx::Size(100, 1), gfx::Size(3, 100),
        gfx::Size(100, 3), gfx::Size(3, 3)}) {
    SkBitmap image;
    image.allocN32Pixels(size.width(), size.height());
    image.eraseColor(SK_ColorWHITE);

    base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>>
        future;
    barcode_detector->Detect(image, future.GetCallback());
    EXPECT_TRUE(future.Get().empty()) << size.ToString();
  }
}

TEST_F(BarcodeDetectionImplChromeTest, RejectsOversizedDimensions) {
  mojo::Remote<mojom::BarcodeDetection> barcode_detector =
      ConnectToBarcodeDetector();

  SkBitmap qr_image = LoadTestImage(FILE_PATH_LITERAL("qr_code.png"));
  ASSERT_GT(qr_image.width(), 0);
  ASSERT_GT(qr_image.height(), 0);

  // Embed a valid QR code into oversized canvases to confirm that detection is
  // rejected by the dimension guard rather than returning empty on blank data.
  for (const gfx::Size& size :
       {gfx::Size(2049, qr_image.height()), gfx::Size(qr_image.width(), 2049),
        gfx::Size(2049, 2049)}) {
    SkBitmap canvas_bitmap;
    canvas_bitmap.allocN32Pixels(size.width(), size.height());
    canvas_bitmap.eraseColor(SK_ColorWHITE);
    SkCanvas canvas(canvas_bitmap);
    canvas.drawImage(qr_image.asImage(), 0, 0);

    base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>>
        future;
    barcode_detector->Detect(canvas_bitmap, future.GetCallback());
    EXPECT_TRUE(future.Get().empty()) << size.ToString();
  }
}

TEST_F(BarcodeDetectionImplChromeTest, AcceptsBoundaryDimensions) {
  mojo::Remote<mojom::BarcodeDetection> barcode_detector =
      ConnectToBarcodeDetector();

  // Lower boundary and extreme aspect ratios are accepted without crashing.
  for (const gfx::Size& size :
       {gfx::Size(4, 4), gfx::Size(2048, 4), gfx::Size(4, 2048)}) {
    SkBitmap min_image;
    min_image.allocN32Pixels(size.width(), size.height());
    min_image.eraseColor(SK_ColorWHITE);

    base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>>
        future;
    barcode_detector->Detect(min_image, future.GetCallback());
    EXPECT_TRUE(future.Get().empty()) << size.ToString();
  }

  // Upper boundary (2048px) still decodes an embedded QR code.
  SkBitmap qr_image = LoadTestImage(FILE_PATH_LITERAL("qr_code.png"));
  for (const gfx::Size& size :
       {gfx::Size(2048, qr_image.height()), gfx::Size(qr_image.width(), 2048),
        gfx::Size(2048, 2048)}) {
    SkBitmap canvas_bitmap;
    canvas_bitmap.allocN32Pixels(size.width(), size.height());
    canvas_bitmap.eraseColor(SK_ColorWHITE);
    SkCanvas canvas(canvas_bitmap);
    canvas.drawImage(qr_image.asImage(), 0, 0);

    base::test::TestFuture<std::vector<mojom::BarcodeDetectionResultPtr>>
        future;
    barcode_detector->Detect(canvas_bitmap, future.GetCallback());
    std::vector<mojom::BarcodeDetectionResultPtr> results = future.Take();
    ASSERT_EQ(1u, results.size()) << size.ToString();
    EXPECT_EQ("https://chromium.org", results[0]->raw_value);
  }
}

}  // namespace shape_detection
