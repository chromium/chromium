// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/buffer_validation.h"

#include <fcntl.h>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {
namespace {

base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}

size_t FileSizeForTesting(size_t size) {
  return size;
}

TEST(BufferValidationTest, VerifyGmbHandlePasses) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/2500, /*size=*/625, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/3125, /*size=*/625, GetDummyFD());

  constexpr size_t buffer_size = 2500 + 2 * 625;
  const gfx::Size size(50, 50);

  EXPECT_TRUE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, buffer_size)));
}

TEST(BufferValidationTest, VerifyGmbHandleInvalidType) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::EMPTY_BUFFER;
  const gfx::Size size(50, 50);

  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, size.GetArea() * 2)));
}

TEST(BufferValidationTest, VerifyGmbHandlePlanesCountMatches) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/2500, /*size=*/625, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/3125, /*size=*/625, GetDummyFD());
  constexpr size_t buffer_size = 2500 + 2 * 625;
  const gfx::Size size(50, 50);

  // PIXEL_FORMAT_UYVY has only 1 plane
  // (https://source.chromium.org/chromium/chromium/src/+/main:media/base/video_frame_layout.cc;l=46;drc=eb5094ebb2c2a4128d36d27806b286d288965746)
  // The gmb handle will have only 3 planes by default.
  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_UYVY, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, buffer_size)));
}

TEST(BufferValidationTest, VerifyGmbHandleStridesMonotonicallyDecrease) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  // Make strides increase. Normal expectation is that they monotonically
  // decrease.
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/60, /*offset=*/2500, /*size=*/1500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/70, /*offset=*/4000, /*size=*/1750, GetDummyFD());
  constexpr size_t buffer_size = 2500 + 1500 + 1750;
  const gfx::Size size(50, 50);

  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, buffer_size)));
}

TEST(BufferValidationTest, VerifyGmbHandleInvalidPlaneHeight) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/2500, /*size=*/625, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/3125, /*size=*/625, GetDummyFD());
  constexpr size_t buffer_size = 2500 + 2 * 625;
  // Make the height greater than the planes'.
  const gfx::Size size(50, 100);

  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, buffer_size)));
}

TEST(BufferValidationTest, VerifyGmbHandleInvalidPlaneWidth) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/2500, /*size=*/625, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/3125, /*size=*/625, GetDummyFD());
  constexpr size_t buffer_size = 2500 + 2 * 625;
  // Make the width greater than the planes'.
  const gfx::Size size(100, 50);

  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, buffer_size)));
}

TEST(BufferValidationTest, VerifyGmbHandleOffsetValid) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/50, /*offset=*/0, /*size=*/2500, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/2500, /*size=*/625, GetDummyFD());
  gmb_handle.native_pixmap_handle.planes.emplace_back(
      /*stride=*/25, /*offset=*/3125, /*size=*/625, GetDummyFD());
  const gfx::Size size(50, 50);

  // Make the file size less than the plane size.
  EXPECT_FALSE(VerifyGpuMemoryBufferHandle(
      PIXEL_FORMAT_I420, size, gmb_handle,
      base::BindRepeating(FileSizeForTesting, 2499)));
}

}  // namespace
}  // namespace media
