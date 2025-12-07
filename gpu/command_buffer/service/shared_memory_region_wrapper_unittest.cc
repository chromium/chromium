// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format) {
  return SharedMemoryImageBackingFactory::CreateGpuMemoryBufferHandle(size,
                                                                      format);
}

TEST(SharedMemoryRegionWrapperTest, SinglePlaneRGBA_8888) {
  constexpr gfx::Size size(100, 100);
  constexpr viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;

  auto handle = CreateGpuMemoryBufferHandle(size, format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, format));
}

TEST(SharedMemoryRegionWrapperTest, MultiPlaneY_UV_420) {
  constexpr gfx::Size size(100, 100);
  constexpr viz::SharedImageFormat format = viz::MultiPlaneFormat::kNV12;

  auto handle = CreateGpuMemoryBufferHandle(size, format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, format));

  uint8_t* memory_y_plane = wrapper.GetMemory(0);
  uint8_t* memory_uv_plane = wrapper.GetMemory(1);

  // UV plane should be 100*100*1 bytes after Y plane.
  size_t offset = memory_uv_plane - memory_y_plane;
  EXPECT_EQ(offset, 10000u);
}

TEST(SharedMemoryRegionWrapperTest, MultiPlaneY_V_U_420) {
  constexpr gfx::Size size(100, 100);
  constexpr viz::SharedImageFormat format = viz::MultiPlaneFormat::kYV12;

  auto handle = CreateGpuMemoryBufferHandle(size, format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, format));

  uint8_t* memory_y_plane = wrapper.GetMemory(0);
  uint8_t* memory_v_plane = wrapper.GetMemory(1);
  uint8_t* memory_u_plane = wrapper.GetMemory(2);

  // V plane should be 100*100*1 bytes after Y plane.
  size_t v_offset = memory_v_plane - memory_y_plane;
  EXPECT_EQ(v_offset, 10000u);

  // U plane should be 50*50*1 bytes after V plane.
  size_t u_offset = memory_u_plane - memory_v_plane;
  EXPECT_EQ(u_offset, 2500u);
}

TEST(SharedMemoryRegionWrapperTest, BufferTooSmallWrongFormat) {
  constexpr gfx::Size size(100, 100);
  constexpr viz::SharedImageFormat format = viz::MultiPlaneFormat::kNV12;

  auto handle = CreateGpuMemoryBufferHandle(size, viz::SinglePlaneFormat::kR_8);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_FALSE(wrapper.Initialize(std::move(handle), size, format));
}

TEST(SharedMemoryRegionWrapperTest, BufferTooSmallWrongSize) {
  constexpr gfx::Size size(100, 100);
  constexpr viz::SharedImageFormat format = viz::MultiPlaneFormat::kNV12;

  auto handle = CreateGpuMemoryBufferHandle(size, format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_FALSE(
      wrapper.Initialize(std::move(handle), gfx::Size(200, 200), format));
}

}  // namespace
}  // namespace gpu
