// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_VIDEO_FRAME_WRITER_H_
#define REMOTING_TEST_VIDEO_FRAME_WRITER_H_

#include "base/time/time.h"

namespace base {
class FilePath;
}

namespace webrtc {
class DesktopFrame;
class DesktopRect;
}  // namespace webrtc

namespace remoting {
namespace test {

// A helper class to dump video frames to disk.
class VideoFrameWriter {
 public:
  VideoFrameWriter();

  VideoFrameWriter(const VideoFrameWriter&) = delete;
  VideoFrameWriter& operator=(const VideoFrameWriter&) = delete;

  ~VideoFrameWriter();

  // Save video frame to a local path.
  void WriteFrameToPath(const webrtc::DesktopFrame& frame,
                        const base::FilePath& image_path);

  // Save video frame to path named with the |instance_creation_time|.
  void WriteFrameToDefaultPath(const webrtc::DesktopFrame& frame);

  // Highlight |rect| on the frame by shifting the RGB value of pixels on the
  // border of |rect|.
  void HighlightRectInFrame(webrtc::DesktopFrame* frame,
                            const webrtc::DesktopRect& rect);

 private:
  // Returns a FilePath by appending the creation time of this object.
  base::FilePath AppendCreationDateAndTime(const base::FilePath& file_path);

  // Returns true if directory already exists or it was created successfully.
  bool CreateDirectoryIfNotExists(const base::FilePath& file_path);

  // Helper function to shift the RGB value of the pixel at location (x, y) by
  // |shift_amount| on each channel.
  static void ShiftPixelColor(webrtc::DesktopFrame* frame,
                              int x,
                              int y,
                              int shift_amount);

  // Used to create a unique folder to dump video frames.
  const base::Time instance_creation_time_;

  // Used to append before file extension to create unique file name.
  int frame_name_unique_number_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_VIDEO_FRAME_WRITER_H_
