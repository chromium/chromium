// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/frame_generator/file_frame_generator.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/codec/png_codec.h"

namespace remoting {

FileFrameGenerator::FileFrameGenerator(const base::FilePath& frame_dir,
                                       const webrtc::DesktopSize& size)
    : frame_dir_(frame_dir), size_(size) {}

FileFrameGenerator::~FileFrameGenerator() = default;

std::unique_ptr<webrtc::DesktopFrame> FileFrameGenerator::GenerateFrame() {
  base::FilePath frame_path = frame_dir_.AppendASCII(
      base::StringPrintf("frame_%04d.png", frame_index_));

  std::string file_contents;
  if (!base::ReadFileToString(frame_path, &file_contents)) {
    VLOG(1) << "End of frame sequence reached or file missing: " << frame_path;
    return nullptr;
  }

  SkBitmap bitmap = gfx::PNGCodec::Decode(base::as_byte_span(file_contents));
  if (bitmap.isNull()) {
    LOG(ERROR) << "Failed to decode PNG frame: " << frame_path;
    return nullptr;
  }

  if (bitmap.width() != size_.width() || bitmap.height() != size_.height()) {
    LOG(ERROR) << "Decoded frame size mismatch. Expected: " << size_.width()
               << "x" << size_.height() << ", Actual: " << bitmap.width() << "x"
               << bitmap.height();
    return nullptr;
  }

  auto frame = std::make_unique<webrtc::BasicDesktopFrame>(size_);

  // Ensure consistent color format (kN32_SkColorType matches DesktopFrame).
  SkImageInfo info = SkImageInfo::Make(size_.width(), size_.height(),
                                       kN32_SkColorType, kPremul_SkAlphaType);

  if (!bitmap.readPixels(info, frame->data(), frame->stride(), 0, 0)) {
    LOG(ERROR) << "Failed to read pixels from bitmap: " << frame_path;
    return nullptr;
  }

  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(size_));
  frame_index_++;
  return frame;
}

}  // namespace remoting
