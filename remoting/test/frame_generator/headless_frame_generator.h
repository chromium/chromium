// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FRAME_GENERATOR_HEADLESS_FRAME_GENERATOR_H_
#define REMOTING_TEST_FRAME_GENERATOR_HEADLESS_FRAME_GENERATOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "remoting/test/frame_generator/frame_generator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class FileFrameGenerator;

// A FrameGenerator that uses Headless Chrome to generate synthetic frames.
// It wraps the Python frame generator script and manages a temporary directory
// for the generated PNG files.
class HeadlessFrameGenerator : public FrameGenerator {
 public:
  HeadlessFrameGenerator(const std::string& scenario,
                         const webrtc::DesktopSize& size,
                         int frame_count,
                         double fps);
  ~HeadlessFrameGenerator() override;

  // FrameGenerator implementation.
  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame() override;

  bool Initialize();

  void SetChromePath(const base::FilePath& chrome_path);

 private:
  std::string scenario_;
  webrtc::DesktopSize size_;
  int frame_count_;
  double fps_;

  base::FilePath chrome_path_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileFrameGenerator> file_generator_;
};

}  // namespace remoting

#endif  // REMOTING_TEST_FRAME_GENERATOR_HEADLESS_FRAME_GENERATOR_H_
