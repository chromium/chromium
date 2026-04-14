// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FRAME_GENERATOR_FILE_FRAME_GENERATOR_H_
#define REMOTING_TEST_FRAME_GENERATOR_FILE_FRAME_GENERATOR_H_

#include <memory>

#include "base/files/file_path.h"
#include "remoting/test/frame_generator/frame_generator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

// A FrameGenerator that reads PNG files from a directory. The files must be
// named "frame_0000.png", "frame_0001.png", etc. and must match the specified
// size. The generator expects a continuous sequence of files; if a file is
// missing, it assumes the end of the sequence has been reached.
class FileFrameGenerator : public FrameGenerator {
 public:
  FileFrameGenerator(const base::FilePath& frame_dir,
                     const webrtc::DesktopSize& size);
  ~FileFrameGenerator() override;

  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame() override;

 private:
  base::FilePath frame_dir_;
  webrtc::DesktopSize size_;
  int frame_index_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FRAME_GENERATOR_FILE_FRAME_GENERATOR_H_
