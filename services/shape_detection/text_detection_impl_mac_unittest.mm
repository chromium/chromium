// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/text_detection_impl_mac.h"

#import <AppKit/AppKit.h>

#include <memory>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gl/gl_switches.h"

using base::test::RunOnceClosure;

namespace shape_detection {

class TextDetectionImplMacTest : public ::testing::Test {
 public:
  ~TextDetectionImplMacTest() override = default;

  void DetectCallback(std::vector<mojom::TextDetectionResultPtr> results) {
    // CIDetectorTypeText doesn't return the decoded text, juts bounding boxes.
    Detection(results.size());
  }
  MOCK_METHOD1(Detection, void(size_t));

  std::unique_ptr<TextDetectionImplMac> impl_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(TextDetectionImplMacTest, CreateAndDestroy) {}

// This test generates an image with a single text line and scans it back.
TEST_F(TextDetectionImplMacTest, ScanOnce) {
  // Text detection needs GPU infrastructure.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseGpuInTests)) {
    return;
  }

  impl_ = std::make_unique<TextDetectionImplMac>();
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> rgb_colorspace(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));

  const int width = 200;
  const int height = 50;
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      nullptr, width, height, 8 /* bitsPerComponent */,
      width * 4 /* rowBytes */, rgb_colorspace.get(),
      uint32_t{kCGImageAlphaPremultipliedFirst} | kCGBitmapByteOrder32Host));

  // Draw a white background.
  CGContextSetRGBFillColor(context.get(), 1.0, 1.0, 1.0, 1.0);
  CGContextFillRect(context.get(), CGRectMake(0.0, 0.0, width, height));

  // Create a line of Helvetica 16 text, and draw it in the |context|.
  NSDictionary* attributes =
      @{NSFontAttributeName : [NSFont fontWithName:@"Helvetica" size:16]};

  NSAttributedString* info =
      [[NSAttributedString alloc] initWithString:@"https://www.chromium.org"
                                      attributes:attributes];

  base::apple::ScopedCFTypeRef<CTLineRef> line(
      CTLineCreateWithAttributedString(base::apple::NSToCFPtrCast(info)));

  CGContextSetTextPosition(context.get(), 10.0, height / 2.0);
  CTLineDraw(line.get(), context.get());

  // Extract a CGImage and its raw pixels from |context|.
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context.get()));
  EXPECT_EQ(static_cast<size_t>(width), CGImageGetWidth(cg_image.get()));
  EXPECT_EQ(static_cast<size_t>(height), CGImageGetHeight(cg_image.get()));

  SkBitmap bitmap;
  ASSERT_TRUE(SkCreateBitmapFromCGImage(&bitmap, cg_image.get()));

  base::RunLoop run_loop;
  // Send the image to Detect() and expect the response in callback.
  EXPECT_CALL(*this, Detection(1))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  impl_->Detect(bitmap,
                base::BindOnce(&TextDetectionImplMacTest::DetectCallback,
                               base::Unretained(this)));

  run_loop.Run();
}

}  // shape_detection namespace
