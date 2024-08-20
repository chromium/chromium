// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/test/frame_generator_util.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/codec/png_codec.h"

namespace {
// dst_stride is int, not size_t, as it may be negative for inverted (in Y)
// images.
void CopyPixelsToBuffer(const SkBitmap& src,
                        uint8_t* dst_pixels,
                        int dst_stride) {
  const char* src_pixels = static_cast<const char*>(src.getPixels());
  size_t src_stride = src.rowBytes();
  // Only need to copy the important parts of the row.
  size_t bytes_per_row = src.width() * src.bytesPerPixel();
  for (int y = 0; y < src.height(); ++y) {
    memcpy(dst_pixels, src_pixels, bytes_per_row);
    src_pixels += src_stride;
    dst_pixels += dst_stride;
  }
}
}  // namespace

namespace remoting {
namespace test {

std::unique_ptr<webrtc::DesktopFrame> LoadDesktopFrameFromPng(
    const char* name) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.AppendASCII("remoting");
  file_path = file_path.AppendASCII("test");
  file_path = file_path.AppendASCII("data");
  file_path = file_path.AppendASCII(name);

  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    LOG(FATAL) << "Failed to read " << file_path.MaybeAsASCII()
               << ". Please run remoting/test/data/download.sh";
  }
  SkBitmap bitmap;
  gfx::PNGCodec::Decode(reinterpret_cast<const uint8_t*>(file_content.data()),
                        file_content.size(), &bitmap);
  std::unique_ptr<webrtc::DesktopFrame> frame(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(bitmap.width(), bitmap.height())));
  CopyPixelsToBuffer(bitmap, frame->data(), frame->stride());
  return frame;
}

void DrawRect(webrtc::DesktopFrame* frame,
              webrtc::DesktopRect rect,
              uint32_t color) {
  for (int y = rect.top(); y < rect.bottom(); ++y) {
    uint32_t* data = reinterpret_cast<uint32_t*>(
        frame->GetFrameDataAtPos(webrtc::DesktopVector(rect.left(), y)));
    for (int x = 0; x < rect.width(); ++x) {
      data[x] = color;
    }
  }
}

}  // namespace test
}  // namespace remoting
