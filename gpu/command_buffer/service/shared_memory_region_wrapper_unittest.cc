// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_memory_region_wrapper.h"

#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(const gfx::Size& size,
                                                 gfx::BufferFormat format) {
  return GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId{1}, size, format,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE);
}

TEST(SharedMemoryRegionWrapperTest, SinglePlaneRGBA_8888) {
  constexpr gfx::Size size(100, 100);
  constexpr gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;

  auto handle = CreateGpuMemoryBuffer(size, buffer_format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, buffer_format));
}

TEST(SharedMemoryRegionWrapperTest, MultiPlaneY_UV_420) {
  constexpr gfx::Size size(100, 100);
  constexpr gfx::BufferFormat buffer_format =
      gfx::BufferFormat::YUV_420_BIPLANAR;

  auto handle = CreateGpuMemoryBuffer(size, buffer_format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, buffer_format));

  uint8_t* memory_y_plane = wrapper.GetMemory(0);
  uint8_t* memory_uv_plane = wrapper.GetMemory(1);

  // UV plane should be 100*100*1 bytes after Y plane.
  size_t offset = memory_uv_plane - memory_y_plane;
  EXPECT_EQ(offset, 10000u);
}

TEST(SharedMemoryRegionWrapperTest, MultiPlaneY_V_U_420) {
  constexpr gfx::Size size(100, 100);
  constexpr gfx::BufferFormat buffer_format = gfx::BufferFormat::YVU_420;

  auto handle = CreateGpuMemoryBuffer(size, buffer_format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_TRUE(wrapper.Initialize(std::move(handle), size, buffer_format));

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
  constexpr gfx::BufferFormat buffer_format =
      gfx::BufferFormat::YUV_420_BIPLANAR;

  auto handle = CreateGpuMemoryBuffer(size, gfx::BufferFormat::R_8);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_FALSE(wrapper.Initialize(std::move(handle), size, buffer_format));
}

TEST(SharedMemoryRegionWrapperTest, BufferTooSmallWrongSize) {
  constexpr gfx::Size size(100, 100);
  constexpr gfx::BufferFormat buffer_format =
      gfx::BufferFormat::YUV_420_BIPLANAR;

  auto handle = CreateGpuMemoryBuffer(size, buffer_format);

  SharedMemoryRegionWrapper wrapper;
  EXPECT_FALSE(wrapper.Initialize(std::move(handle), gfx::Size(200, 200),
                                  buffer_format));
}

}  // namespace
}  // namespace gpu
