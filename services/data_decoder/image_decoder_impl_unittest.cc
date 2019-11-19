// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "gin/array_buffer.h"
#include "gin/public/isolate_holder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/data_decoder/image_decoder_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#include "gin/v8_initializer.h"
#endif

namespace data_decoder {

namespace {

const int64_t kTestMaxImageSize = 128 * 1024;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
#endif

bool CreateJPEGImage(int width,
                     int height,
                     SkColor color,
                     std::vector<unsigned char>* output) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);

  constexpr int kQuality = 50;
  if (!gfx::JPEGCodec::Encode(bitmap, kQuality, output)) {
    LOG(ERROR) << "Unable to encode " << width << "x" << height << " bitmap";
    return false;
  }
  return true;
}

class Request {
 public:
  explicit Request(ImageDecoderImpl* decoder) : decoder_(decoder) {}

  void DecodeImage(const std::vector<unsigned char>& image, bool shrink) {
    decoder_->DecodeImage(
        image, mojom::ImageCodec::DEFAULT, shrink, kTestMaxImageSize,
        gfx::Size(),  // Take the smallest frame (there's only one frame).
        base::Bind(&Request::OnRequestDone, base::Unretained(this)));
  }

  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  void OnRequestDone(const SkBitmap& result_image) { bitmap_ = result_image; }

  ImageDecoderImpl* decoder_;
  SkBitmap bitmap_;
};

// We need to ensure that Blink and V8 are initialized in order to use content's
// image decoding call.
class BlinkInitializer : public blink::Platform {
 public:
  BlinkInitializer() {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

    mojo::BinderMap binders;
    blink::CreateMainThreadAndInitialize(this, &binders);
  }

  ~BlinkInitializer() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkInitializer);
};

base::LazyInstance<BlinkInitializer>::Leaky g_blink_initializer =
    LAZY_INSTANCE_INITIALIZER;

class ImageDecoderImplTest : public testing::Test {
 public:
  ImageDecoderImplTest() = default;
  ~ImageDecoderImplTest() override = default;

  void SetUp() override { g_blink_initializer.Get(); }

 protected:
  ImageDecoderImpl* decoder() { return &decoder_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ImageDecoderImpl decoder_;
};

}  // namespace

// Test that DecodeImage() doesn't return image message > (max message size)
TEST_F(ImageDecoderImplTest, DecodeImageSizeLimit) {
  // Approx max height for 3:2 image that will fit in the allotted space.
  // 1.5 for width/height ratio, 4 for bytes/pixel.
  int max_height_for_msg = sqrt(kTestMaxImageSize / (1.5 * 4));
  int base_msg_size = sizeof(skia::mojom::Bitmap::Data_);

  // Sizes which should trigger dimension-halving 0, 1 and 2 times
  int heights[] = {max_height_for_msg - 10, max_height_for_msg + 10,
                   2 * max_height_for_msg + 10};
  int widths[] = {heights[0] * 3 / 2, heights[1] * 3 / 2, heights[2] * 3 / 2};
  for (size_t i = 0; i < base::size(heights); i++) {
    std::vector<unsigned char> jpg;
    ASSERT_TRUE(CreateJPEGImage(widths[i], heights[i], SK_ColorRED, &jpg));

    Request request(decoder());
    request.DecodeImage(jpg, true);
    ASSERT_FALSE(request.bitmap().isNull());

    // Check that image has been shrunk appropriately
    EXPECT_LT(request.bitmap().computeByteSize() + base_msg_size,
              static_cast<uint64_t>(kTestMaxImageSize));
// Android does its own image shrinking for memory conservation deeper in
// the decode, so more specific tests here won't work.
#if !defined(OS_ANDROID)
    EXPECT_EQ(widths[i] >> i, request.bitmap().width());
    EXPECT_EQ(heights[i] >> i, request.bitmap().height());

    // Check that if resize not requested and image exceeds IPC size limit,
    // an empty image is returned
    if (heights[i] > max_height_for_msg) {
      Request request(decoder());
      request.DecodeImage(jpg, false);
      EXPECT_TRUE(request.bitmap().isNull());
    }
#endif
  }
}

TEST_F(ImageDecoderImplTest, DecodeImageFailed) {
  // The "jpeg" is just some "random" data;
  const char kRandomData[] = "u gycfy7xdjkhfgui bdui ";
  std::vector<unsigned char> jpg(kRandomData,
                                 kRandomData + sizeof(kRandomData));

  Request request(decoder());
  request.DecodeImage(jpg, false);
  EXPECT_TRUE(request.bitmap().isNull());
}

}  // namespace data_decoder
