// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/frame_generator/differ_frame_generator.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/differ_block.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

DifferFrameGenerator::DifferFrameGenerator(
    std::unique_ptr<FrameGenerator> base_generator)
    : base_generator_(std::move(base_generator)) {}

DifferFrameGenerator::~DifferFrameGenerator() = default;

std::unique_ptr<webrtc::DesktopFrame> DifferFrameGenerator::GenerateFrame() {
  std::unique_ptr<webrtc::DesktopFrame> frame =
      base_generator_->GenerateFrame();
  if (!frame) {
    return nullptr;
  }

  webrtc::DesktopRegion* updated_region = frame->mutable_updated_region();
  updated_region->Clear();

  if (!last_frame_ || !last_frame_->size().equals(frame->size()) ||
      last_frame_->stride() != frame->stride()) {
    updated_region->AddRect(webrtc::DesktopRect::MakeSize(frame->size()));
  } else {
    // Compare blocks.
    const int width = frame->size().width();
    const int height = frame->size().height();
    const int stride = frame->stride();
    CHECK_GT(stride, 0);

    // SAFETY: webrtc::DesktopFrame::data() returns a buffer of size at least
    // stride * height. This is safe as long as we don't access beyond that.
    // We use base::span to ensure all subsequent accesses are bounds-checked.
    auto data_span = UNSAFE_BUFFERS(
        base::span(frame->data(), static_cast<size_t>(stride) * height));
    // SAFETY: webrtc::DesktopFrame::data() returns a buffer of size at least
    // stride * height. Since we checked that last_frame_->stride() matches
    // frame->stride(), this size is correct for the last frame as well.
    auto last_data_span = UNSAFE_BUFFERS(
        base::span(last_frame_->data(), static_cast<size_t>(stride) * height));

    for (int y = 0; y < height; y += webrtc::kBlockSize) {
      int block_height = std::min(webrtc::kBlockSize, height - y);
      for (int x = 0; x < width; x += webrtc::kBlockSize) {
        int block_width = std::min(webrtc::kBlockSize, width - x);

        // Use size_t for all operands to prevent integer overflow.
        size_t offset = static_cast<size_t>(y) * static_cast<size_t>(stride) +
                        static_cast<size_t>(x) *
                            static_cast<size_t>(webrtc::kBytesPerPixel);

        bool changed = false;
        if (block_width < webrtc::kBlockSize) {
          // BlockDifference() assumes kBlockSize width. If we are at the right
          // edge of the frame and the block is narrower than kBlockSize, we
          // mark it as changed to avoid out-of-bounds memory access.
          // Note: webrtc::BlockDifference can safely handle partial heights, so
          // we only need to be conservative for narrow widths.
          changed = true;
        } else {
          // webrtc::BlockDifference requires raw pointers. subspan() ensures
          // the offset and size are within bounds of the original span.
          changed = webrtc::BlockDifference(
              data_span.subspan(offset).data(),
              last_data_span.subspan(offset).data(), block_height, stride);
        }

        if (changed) {
          updated_region->AddRect(
              webrtc::DesktopRect::MakeXYWH(x, y, block_width, block_height));
        }
      }
    }
  }

  last_frame_ = webrtc::SharedDesktopFrame::Wrap(std::move(frame));
  return last_frame_->Share();
}

}  // namespace remoting
