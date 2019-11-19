// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "media/cast/test/utility/barcode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {
namespace {

TEST(BarcodeTest, Small) {
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(320, 10));
  for (unsigned char in_bits = 0; in_bits < 255; in_bits++) {
    EXPECT_TRUE(EncodeBarcode(in_bits, frame));
    unsigned char out_bits = ~in_bits;
    EXPECT_TRUE(DecodeBarcode(*frame, &out_bits));
    EXPECT_EQ(in_bits, out_bits);
  }
}

TEST(BarcodeTest, Large) {
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10000, 10));
  std::vector<bool> in_bits(1024);
  std::vector<bool> out_bits(1024);
  for (int i = 0; i < 1024; i++) in_bits[i] = true;
  EXPECT_TRUE(EncodeBarcode(in_bits, frame));
  EXPECT_TRUE(DecodeBarcode(*frame, &out_bits));
  for (int i = 0; i < 1024; i++) {
    EXPECT_EQ(in_bits[i], out_bits[i]);
  }

  for (int i = 0; i < 1024; i++) in_bits[i] = false;
  EXPECT_TRUE(EncodeBarcode(in_bits, frame));
  EXPECT_TRUE(DecodeBarcode(*frame, &out_bits));
  for (int i = 0; i < 1024; i++) {
    EXPECT_EQ(in_bits[i], out_bits[i]);
  }

  for (int i = 0; i < 1024; i++) in_bits[i] = (i & 1) == 0;
  EXPECT_TRUE(EncodeBarcode(in_bits, frame));
  EXPECT_TRUE(DecodeBarcode(*frame, &out_bits));
  for (int i = 0; i < 1024; i++) {
    EXPECT_EQ(in_bits[i], out_bits[i]);
  }

  for (int i = 0; i < 1024; i++) in_bits[i] = (i & 1) == 1;
  EXPECT_TRUE(EncodeBarcode(in_bits, frame));
  EXPECT_TRUE(DecodeBarcode(*frame, &out_bits));
  for (int i = 0; i < 1024; i++) {
    EXPECT_EQ(in_bits[i], out_bits[i]);
  }
}

}  // namespace
}  // namespace test
}  // namespace cast
}  // namespace media
