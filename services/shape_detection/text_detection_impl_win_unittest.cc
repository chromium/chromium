// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"
#include "services/shape_detection/text_detection_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

namespace shape_detection {

namespace {

void DetectTextCallback(base::OnceClosure quit_closure,
                        std::vector<mojom::TextDetectionResultPtr>* results_out,
                        std::vector<mojom::TextDetectionResultPtr> results_in) {
  *results_out = std::move(results_in);
  std::move(quit_closure).Run();
}

}  // namespace

class TextDetectionImplWinTest : public testing::Test {
 public:
  TextDetectionImplWinTest(const TextDetectionImplWinTest&) = delete;
  TextDetectionImplWinTest& operator=(const TextDetectionImplWinTest&) = delete;

 protected:
  TextDetectionImplWinTest() = default;
  ~TextDetectionImplWinTest() override = default;

  void SetUp() override {
    scoped_com_initializer_ = std::make_unique<base::win::ScopedCOMInitializer>(
        base::win::ScopedCOMInitializer::kMTA);
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());
  }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(TextDetectionImplWinTest, ScanOnce) {
  mojo::Remote<mojom::TextDetection> text_service;
  TextDetectionImpl::Create(text_service.BindNewPipeAndPassReceiver());

  // Load image data from test directory.
  base::FilePath image_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &image_path));
  image_path = image_path.Append(FILE_PATH_LITERAL("services"))
                   .Append(FILE_PATH_LITERAL("test"))
                   .Append(FILE_PATH_LITERAL("data"))
                   .Append(FILE_PATH_LITERAL("text_detection.png"));
  ASSERT_TRUE(base::PathExists(image_path));
  std::string image_data;
  ASSERT_TRUE(base::ReadFileToString(image_path, &image_data));

  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const uint8_t*>(image_data.data()),
                        image_data.size(), &bitmap);

  const gfx::Size size(bitmap.width(), bitmap.height());
  const uint32_t num_bytes = size.GetArea() * 4 /* bytes per pixel */;
  ASSERT_EQ(num_bytes, bitmap.computeByteSize());

  base::RunLoop run_loop;
  std::vector<mojom::TextDetectionResultPtr> results;
  text_service->Detect(
      bitmap,
      base::BindOnce(&DetectTextCallback, run_loop.QuitClosure(), &results));
  run_loop.Run();
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ("The Chromium Project website is:", results[0]->raw_value);
  EXPECT_EQ(gfx::RectF(51, 38, 272, 17), results[0]->bounding_box);
  EXPECT_EQ("https://www.chromium.org", results[1]->raw_value);
  EXPECT_EQ(gfx::RectF(51, 63, 209, 17), results[1]->bounding_box);
}

}  // namespace shape_detection
