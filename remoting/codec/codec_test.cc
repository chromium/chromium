// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/codec_test.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/base/util.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopRegion;
using webrtc::DesktopSize;

namespace {

const int kBytesPerPixel = 4;

// Some sample rects for testing.
std::vector<DesktopRegion> MakeTestRegionLists(DesktopSize size) {
  std::vector<DesktopRegion> region_lists;
  DesktopRegion region;
  region.AddRect(DesktopRect::MakeXYWH(0, 0, size.width(), size.height()));
  region_lists.push_back(region);
  region.Clear();
  region.AddRect(
      DesktopRect::MakeXYWH(0, 0, size.width() / 2, size.height() / 2));
  region_lists.push_back(region);
  region.Clear();
  region.AddRect(DesktopRect::MakeXYWH(size.width() / 2, size.height() / 2,
                                       size.width() / 2, size.height() / 2));
  region_lists.push_back(region);
  region.Clear();
  region.AddRect(DesktopRect::MakeXYWH(16, 16, 16, 16));
  region.AddRect(DesktopRect::MakeXYWH(32, 32, 32, 32));
  region.IntersectWith(DesktopRect::MakeSize(size));
  region_lists.push_back(region);
  return region_lists;
}

}  // namespace

namespace remoting {

class VideoDecoderTester {
 public:
  VideoDecoderTester(VideoDecoder* decoder, const DesktopSize& screen_size)
      : strict_(false),
        decoder_(decoder),
        frame_(new BasicDesktopFrame(screen_size)),
        expected_frame_(nullptr) {}

  void Reset() {
    frame_.reset(new BasicDesktopFrame(frame_->size()));
    expected_region_.Clear();
  }

  void ResetRenderedData() {
    memset(frame_->data(), 0,
           frame_->size().width() * frame_->size().height() * kBytesPerPixel);
  }

  void ReceivedPacket(std::unique_ptr<VideoPacket> packet) {
    ASSERT_TRUE(decoder_->DecodePacket(*packet, frame_.get()));
  }

  void set_strict(bool strict) {
    strict_ = strict;
  }

  void set_expected_frame(DesktopFrame* frame) {
    expected_frame_ = frame;
  }

  void AddRegion(const DesktopRegion& region) {
    expected_region_.AddRegion(region);
  }

  void VerifyResults() {
    if (!strict_)
      return;

    ASSERT_TRUE(expected_frame_);

    // Test the content of the update region.
    EXPECT_TRUE(expected_region_.Equals(frame_->updated_region()));

    for (DesktopRegion::Iterator i(frame_->updated_region()); !i.IsAtEnd();
         i.Advance()) {
      const uint8_t* original =
          expected_frame_->GetFrameDataAtPos(i.rect().top_left());
      const uint8_t* decoded =
          frame_->GetFrameDataAtPos(i.rect().top_left());
      const int row_size = kBytesPerPixel * i.rect().width();
      for (int y = 0; y < i.rect().height(); ++y) {
        EXPECT_EQ(0, memcmp(original, decoded, row_size))
            << "Row " << y << " is different";
        original += expected_frame_->stride();
        decoded += frame_->stride();
      }
    }
  }

  // The error at each pixel is the root mean square of the errors in
  // the R, G, and B components, each normalized to [0, 1]. This routine
  // checks that the maximum and mean pixel errors do not exceed given limits.
  void VerifyResultsApprox(double max_error_limit, double mean_error_limit) {
    double max_error = 0.0;
    double sum_error = 0.0;
    int error_num = 0;
    for (DesktopRegion::Iterator i(frame_->updated_region()); !i.IsAtEnd();
         i.Advance()) {
      const uint8_t* expected =
          expected_frame_->GetFrameDataAtPos(i.rect().top_left());
      const uint8_t* actual =
          frame_->GetFrameDataAtPos(i.rect().top_left());
      for (int y = 0; y < i.rect().height(); ++y) {
        for (int x = 0; x < i.rect().width(); ++x) {
          double error = CalculateError(expected + x * kBytesPerPixel,
                                        actual + x * kBytesPerPixel);
          max_error = std::max(max_error, error);
          sum_error += error;
          ++error_num;
        }
        expected += expected_frame_->stride();
        actual += frame_->stride();
      }
    }
    EXPECT_LE(max_error, max_error_limit);
    double mean_error = sum_error / error_num;
    EXPECT_LE(mean_error, mean_error_limit);
    VLOG(0) << "Max error: " << max_error;
    VLOG(0) << "Mean error: " << mean_error;
  }

  double CalculateError(const uint8_t* original, const uint8_t* decoded) {
    double error_sum_squares = 0.0;
    for (int i = 0; i < 3; i++) {
      double error = static_cast<double>(*original++) -
                     static_cast<double>(*decoded++);
      error /= 255.0;
      error_sum_squares += error * error;
    }
    original++;
    decoded++;
    return sqrt(error_sum_squares / 3.0);
  }

 private:
  bool strict_;
  DesktopRegion expected_region_;
  VideoDecoder* decoder_;
  std::unique_ptr<DesktopFrame> frame_;
  DesktopFrame* expected_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderTester);
};

// The VideoEncoderTester provides a hook for retrieving the data, and passing
// the message to other subprograms for validaton.
class VideoEncoderTester {
 public:
  VideoEncoderTester() : decoder_tester_(nullptr), data_available_(0) {}

  ~VideoEncoderTester() {
    EXPECT_GT(data_available_, 0);
  }

  void DataAvailable(std::unique_ptr<VideoPacket> packet) {
    ++data_available_;
    // Send the message to the VideoDecoderTester.
    if (decoder_tester_) {
      decoder_tester_->ReceivedPacket(std::move(packet));
    }
  }

  void set_decoder_tester(VideoDecoderTester* decoder_tester) {
    decoder_tester_ = decoder_tester;
  }

 private:
  VideoDecoderTester* decoder_tester_;
  int data_available_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderTester);
};

std::unique_ptr<DesktopFrame> PrepareFrame(const DesktopSize& size) {
  std::unique_ptr<DesktopFrame> frame(new BasicDesktopFrame(size));

  srand(0);
  int memory_size = size.width() * size.height() * kBytesPerPixel;
  for (int i = 0; i < memory_size; ++i) {
    frame->data()[i] = rand() % 256;
  }

  return frame;
}

static void TestEncodingRects(VideoEncoder* encoder,
                              VideoEncoderTester* tester,
                              DesktopFrame* frame,
                              const DesktopRegion& region) {
  *frame->mutable_updated_region() = region;
  tester->DataAvailable(encoder->Encode(*frame));
}

void TestVideoEncoder(VideoEncoder* encoder, bool strict) {
  const int kSizes[] = {80, 79, 77, 54};

  VideoEncoderTester tester;

  for (size_t xi = 0; xi < base::size(kSizes); ++xi) {
    for (size_t yi = 0; yi < base::size(kSizes); ++yi) {
      DesktopSize size(kSizes[xi], kSizes[yi]);
      std::unique_ptr<DesktopFrame> frame = PrepareFrame(size);
      for (const DesktopRegion& region : MakeTestRegionLists(size)) {
        TestEncodingRects(encoder, &tester, frame.get(), region);
      }

      // Pass some empty frames through the encoder.
      for (int i = 0; i < 5; ++i) {
        TestEncodingRects(encoder, &tester, frame.get(), DesktopRegion());
      }
    }
  }
}

void TestVideoEncoderEmptyFrames(VideoEncoder* encoder,
                                 int max_topoff_frames) {
  const DesktopSize kSize(100, 100);
  std::unique_ptr<DesktopFrame> frame(PrepareFrame(kSize));

  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(kSize));
  EXPECT_TRUE(encoder->Encode(*frame));

  int topoff_frames = 0;
  frame->mutable_updated_region()->Clear();
  for (int i = 0; i < max_topoff_frames + 1; ++i) {
    if (!encoder->Encode(*frame))
      break;
    topoff_frames++;
  }

  // If top-off is enabled then our random frame contents should always
  // trigger it, so expect at least one top-off frame - strictly, though,
  // an encoder may not always need to top-off.
  EXPECT_GE(topoff_frames, max_topoff_frames ? 1 : 0);
  EXPECT_LE(topoff_frames, max_topoff_frames);
}

static void TestEncodeDecodeRects(VideoEncoder* encoder,
                                  VideoEncoderTester* encoder_tester,
                                  VideoDecoderTester* decoder_tester,
                                  DesktopFrame* frame,
                                  const DesktopRegion& region) {
  *frame->mutable_updated_region() = region;
  decoder_tester->AddRegion(region);

  // Generate random data for the updated region.
  srand(0);
  for (DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    const int row_size = DesktopFrame::kBytesPerPixel * i.rect().width();
    uint8_t* memory = frame->data() + frame->stride() * i.rect().top() +
                    DesktopFrame::kBytesPerPixel * i.rect().left();
    for (int y = 0; y < i.rect().height(); ++y) {
      for (int x = 0; x < row_size; ++x)
        memory[x] = rand() % 256;
      memory += frame->stride();
    }
  }

  encoder_tester->DataAvailable(encoder->Encode(*frame));
  decoder_tester->VerifyResults();
  decoder_tester->Reset();
}

void TestVideoEncoderDecoder(VideoEncoder* encoder,
                             VideoDecoder* decoder,
                             bool strict) {
  DesktopSize kSize = DesktopSize(160, 120);

  VideoEncoderTester encoder_tester;

  std::unique_ptr<DesktopFrame> frame = PrepareFrame(kSize);

  VideoDecoderTester decoder_tester(decoder, kSize);
  decoder_tester.set_strict(strict);
  decoder_tester.set_expected_frame(frame.get());
  encoder_tester.set_decoder_tester(&decoder_tester);

  for (const DesktopRegion& region : MakeTestRegionLists(kSize)) {
    TestEncodeDecodeRects(encoder, &encoder_tester, &decoder_tester,
                          frame.get(), region);
  }
}

static void FillWithGradient(DesktopFrame* frame) {
  for (int j = 0; j < frame->size().height(); ++j) {
    uint8_t* p = frame->data() + j * frame->stride();
    for (int i = 0; i < frame->size().width(); ++i) {
      *p++ = (255.0 * i) / frame->size().width();
      *p++ = (164.0 * j) / frame->size().height();
      *p++ = (82.0 * (i + j)) /
          (frame->size().width() + frame->size().height());
      *p++ = 0;
    }
  }
}

void TestVideoEncoderDecoderGradient(VideoEncoder* encoder,
                                     VideoDecoder* decoder,
                                     const DesktopSize& screen_size,
                                     double max_error_limit,
                                     double mean_error_limit) {
  std::unique_ptr<BasicDesktopFrame> frame(new BasicDesktopFrame(screen_size));
  FillWithGradient(frame.get());
  frame->mutable_updated_region()->SetRect(DesktopRect::MakeSize(screen_size));

  VideoDecoderTester decoder_tester(decoder, screen_size);
  decoder_tester.set_expected_frame(frame.get());
  decoder_tester.AddRegion(frame->updated_region());
  decoder_tester.ReceivedPacket(encoder->Encode(*frame));

  decoder_tester.VerifyResultsApprox(max_error_limit, mean_error_limit);
}

}  // namespace remoting
