// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_IMAGE_H_
#define MEDIA_GPU_TEST_IMAGE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "media/base/video_transformation.h"
#include "media/base/video_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace test {

// The Image class provides functionality to load image files and manage their
// properties such as format, size, checksums,... Currently only raw yuv files
// are supported.
class Image {
 public:
  explicit Image(const base::FilePath& file_path);

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  ~Image();

  // Load the image file and accompanying metadata from disk.
  bool Load();
  // Returns true if the Image file was loaded.
  bool IsLoaded() const;

  // Load image metadata from the json file accompanying the image file.
  bool LoadMetadata();
  // Return true if image metadata is already loaded.
  bool IsMetadataLoaded() const;

  // Get the image data.
  uint8_t* Data() const;
  // Get the image data size.
  size_t DataSize() const;

  // Get the image pixel format.
  VideoPixelFormat PixelFormat() const;
  // Get the image size.
  const gfx::Size& Size() const;
  // Get the visible rectangle of the image.
  const gfx::Rect& VisibleRect() const;
  // Get the image rotation info.
  VideoRotation Rotation() const;
  // Get the image checksum.
  const char* Checksum() const;

 private:
  // The image file path, can be absolute or relative to the test data path.
  base::FilePath file_path_;

  // The mapped image data.
  // TODO(dstaessens@) Investigate creating const video frames from const data
  // so we can remove mutable here.
  mutable base::MemoryMappedFile mapped_file_;
  // The image pixel format.
  VideoPixelFormat pixel_format_ = PIXEL_FORMAT_UNKNOWN;
  // The image size.
  gfx::Size size_;
  // The visible rectangle of the image.
  gfx::Rect visible_rect_;
  // The rotation info of image.
  VideoRotation rotation_;
  // The image md5 checksum.
  std::string checksum_;
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_IMAGE_H_
