// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of MappableBuffer should
// pass in order to be conformant.

#ifndef GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_TEST_TEMPLATE_H_
#define GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_TEST_TEMPLATE_H_

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/internal/mappable_buffer_shared_memory.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/mojom/buffer_types.mojom.h"
#include "ui/gl/gl_display.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "gpu/command_buffer/client/internal/mappable_buffer_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/client/internal/mappable_buffer_io_surface.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "gpu/command_buffer/client/internal/mappable_buffer_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/client/internal/mappable_buffer_dxgi.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/client/internal/mappable_buffer_ahb.h"
#endif

namespace gpu {

template <typename MappableBufferType>
class MappableBufferTest : public testing::Test {
 public:
  MappableBufferTest() {
#if BUILDFLAG(IS_OZONE)
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
#endif
  }

  void CreateGpuMemoryBuffer(const gfx::Size& size,
                             viz::SharedImageFormat format,
                             gfx::BufferUsage usage,
                             gfx::GpuMemoryBufferHandle* handle) {
    MappableBufferType::AllocateForTesting(size, format, usage, handle);
  }

  std::unique_ptr<MappableBuffer> CreateMappableBufferFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      viz::SharedImageFormat format,
      gfx::BufferUsage usage) {
    switch (handle.type) {
      case gfx::SHARED_MEMORY_BUFFER:
        return MappableBufferSharedMemory::CreateFromHandleForTesting(
            std::move(handle), size, format, usage);
#if BUILDFLAG(IS_MAC)
      case gfx::IO_SURFACE_BUFFER:
        return MappableBufferIOSurface::CreateFromHandleForTesting(
            std::move(handle), size, format, usage);
#endif
#if BUILDFLAG(IS_OZONE)
      case gfx::NATIVE_PIXMAP:
        return MappableBufferNativePixmap::CreateFromHandleForTesting(
            client_native_pixmap_factory_.get(), std::move(handle), size,
            format, usage);
#endif
#if BUILDFLAG(IS_WIN)
      case gfx::DXGI_SHARED_HANDLE:
        return MappableBufferDXGI::CreateFromHandleForTesting(std::move(handle),
                                                              size, format);
#endif
#if BUILDFLAG(IS_ANDROID)
      case gfx::ANDROID_HARDWARE_BUFFER:
        return MappableBufferAHB::CreateFromHandleForTesting(std::move(handle),
                                                             size, format);
#endif
      default:
        NOTREACHED();
    }
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
  // Overridden from testing::Test:
  void SetUp() override {
    // https://crrev.com/c/5348599
    // GmbImplTestNativePixmap is a no-op, we should run it on a gpu runner.
#if BUILDFLAG(IS_OZONE)
    // TODO(329211602): Currently only wayland has a valid
    // IsNativePixmapConfigSupported(). We should implement that in X11 and
    // other platforms.
    if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
      run_gpu_test_ = true;
    }
#endif

    if (run_gpu_test_) {
#if BUILDFLAG(IS_OZONE)
      // Make Ozone run in single-process mode.
      ui::OzonePlatform::InitParams params;
      params.single_process = true;
      ui::OzonePlatform::InitializeForUI(params);
      ui::OzonePlatform::InitializeForGPU(params);
#endif
    }

    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();

    if (run_gpu_test_) {
      // Initialize the gpu service because wayland needs the service to pass
      // the display events for initialization of supported formats, etc.
      viz::TestGpuServiceHolder::GetInstance();
      // Make sure all the tasks posted to the current task runner by the
      // initialization functions are run before running the tests, for example,
      // WaylandBufferManagerGpu::Initialize.
      base::RunLoop().RunUntilIdle();
    }
  }
  void TearDown() override {
    if (run_gpu_test_) {
      viz::TestGpuServiceHolder::ResetInstance();
    }
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }
#endif

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  bool CheckGpuMemoryBufferHandle(const gfx::GpuMemoryBufferHandle& handle) {
#if BUILDFLAG(IS_OZONE)
    // Pixmap backend could fail to allocate because of platform difference
    // But it is expected behaviour, so we cannot fail.
    // https://chromium-review.googlesource.com/c/chromium/src/+/5348599
#else
    EXPECT_NE(handle.type, gfx::EMPTY_BUFFER);
#endif
    return handle.type != gfx::EMPTY_BUFFER;
  }

  base::span<gfx::BufferUsage> usages() { return usages_; }
  base::span<const viz::SharedImageFormat> formats() {
#if BUILDFLAG(IS_OZONE)
    // Whether a given (usage, format) pair is valid on Ozone is determined
    // dynamically via ui::OzonePlatform::IsNativePixmapConfigSupported(). Here
    // we pass all possibly-valid formats.
    return viz::GetMappableSharedImageFormatForTesting();
#else
    return formats_;
#endif
  }

 private:
  bool run_gpu_test_ = false;
  raw_ptr<gl::GLDisplay> display_ = nullptr;

  // The BufferUsages and SharedImageFormats that are valid to pass when
  // creating a MappableBuffer vary by platform.
#if BUILDFLAG(IS_ANDROID)
  std::array<gfx::BufferUsage, 2> usages_ = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
  };
  std::array<viz::SharedImageFormat, 1> formats_ = {
      viz::MultiPlaneFormat::kNV12,
  };
#elif BUILDFLAG(IS_WIN)
  std::array<gfx::BufferUsage, 2> usages_ = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
  };
  std::array<viz::SharedImageFormat, 4> formats_ = {
      viz::SinglePlaneFormat::kRGBA_8888,
      viz::SinglePlaneFormat::kRGBX_8888,
      viz::SinglePlaneFormat::kBGRA_8888,
      viz::SinglePlaneFormat::kBGRX_8888,
  };
#elif BUILDFLAG(IS_APPLE)
  std::array<gfx::BufferUsage, 6> usages_ = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_FRONT_RENDERING,
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
  };
  std::array<viz::SharedImageFormat, 13> formats_ = {
      viz::SinglePlaneFormat::kRGBA_8888, viz::SinglePlaneFormat::kRGBX_8888,
      viz::SinglePlaneFormat::kBGRA_8888, viz::SinglePlaneFormat::kBGRX_8888,
      viz::SinglePlaneFormat::kR_8,       viz::SinglePlaneFormat::kRG_88,
      viz::SinglePlaneFormat::kR_16,      viz::SinglePlaneFormat::kRG_1616,
      viz::SinglePlaneFormat::kRGBA_F16,  viz::SinglePlaneFormat::kBGRA_1010102,
      viz::MultiPlaneFormat::kNV12,       viz::MultiPlaneFormat::kNV12A,
      viz::MultiPlaneFormat::kP010,
  };
#elif BUILDFLAG(IS_OZONE)
  // Whether a given (usage, format) pair is valid on Ozone is determined
  // dynamically via ui::OzonePlatform::IsNativePixmapConfigSupported(). Here we
  // specify the set of possibly-valid usages. We pass all possibly-valid
  // formats in formats().
  std::array<gfx::BufferUsage, 8> usages_ = {
      gfx::BufferUsage::GPU_READ,
      gfx::BufferUsage::SCANOUT,
      gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
      gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
  };
#endif

#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif
};

TYPED_TEST_SUITE_P(MappableBufferTest);

TYPED_TEST_P(MappableBufferTest, CreateFromHandle) {
  const gfx::Size kBufferSize(8, 8);

  for (auto format : TestFixture::formats()) {
    for (auto usage : TestFixture::usages()) {
#if BUILDFLAG(IS_OZONE)
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
              viz::SharedImageFormatToBufferFormat(format), usage)) {
        continue;
      }
#endif

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);

      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      std::unique_ptr<MappableBuffer> buffer(
          TestFixture::CreateMappableBufferFromHandle(
              std::move(handle), kBufferSize, format, usage));
      ASSERT_TRUE(buffer);
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
TYPED_TEST_P(MappableBufferTest, CreateFromHandleSmallBuffer) {
  const gfx::Size kBufferSize(8, 8);

  for (auto format : TestFixture::formats()) {
    for (auto usage : TestFixture::usages()) {
#if BUILDFLAG(IS_OZONE)
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
              viz::SharedImageFormatToBufferFormat(format), usage)) {
        continue;
      }
#endif

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);
      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      gfx::Size bogus_size = kBufferSize;
      bogus_size.Enlarge(100, 100);

      // Handle import should fail when the size is bigger than expected.
      std::unique_ptr<MappableBuffer> buffer(
          TestFixture::CreateMappableBufferFromHandle(
              std::move(handle), bogus_size, format, usage));

      // Only non-mappable GMB implementations can be imported with invalid
      // size. In other words all GMP implementations that allow memory mapping
      // must validate image size when importing a handle.
      if (buffer) {
        ASSERT_FALSE(buffer->Map());
      }
    }
  }
}

TYPED_TEST_P(MappableBufferTest, Map) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  const gfx::Size kBufferSize(4, 4);

  if (!base::Contains(TestFixture::usages(),
                      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
    GTEST_SKIP();
  }

  for (auto format : TestFixture::formats()) {
#if BUILDFLAG(IS_OZONE)
    if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
        !ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
            viz::SharedImageFormatToBufferFormat(format),
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
      continue;
    }
#endif

    gfx::GpuMemoryBufferHandle handle;
    TestFixture::CreateGpuMemoryBuffer(
        kBufferSize, format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        &handle);
    if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
      continue;
    }

    std::unique_ptr<MappableBuffer> buffer(
        TestFixture::CreateMappableBufferFromHandle(
            std::move(handle), kBufferSize, format,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE));
    ASSERT_TRUE(buffer);

    // Map buffer into user space.
    ASSERT_TRUE(buffer->Map());

    // Map the buffer a second time. This should be a noop and simply allow
    // multiple clients concurrent read access. Likewise a subsequent Unmap()
    // shouldn't invalidate the first's Map().
    ASSERT_TRUE(buffer->Map());
    buffer->Unmap();

    // Copy and compare mapped buffers.
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      const size_t row_size_in_bytes =
          viz::SharedMemoryRowSizeForSharedImageFormat(format, plane,
                                                       kBufferSize.width())
              .value_or(0);
      EXPECT_GT(row_size_in_bytes, 0u);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      UNSAFE_TODO(memset(data.data(), 0x2a + plane, row_size_in_bytes));

      size_t height = format.GetPlaneSize(plane, kBufferSize).height();
      for (size_t y = 0; y < height; ++y) {
        UNSAFE_TODO(memcpy(static_cast<char*>(buffer->memory(plane)) +
                               y * buffer->stride(plane),
                           data.data(), row_size_in_bytes));
        UNSAFE_TODO(
            EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                    y * buffer->stride(plane),
                                data.data(), row_size_in_bytes)));
      }
    }

    buffer->Unmap();
  }
}

TYPED_TEST_P(MappableBufferTest, PersistentMap) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  const gfx::Size kBufferSize(4, 4);

  if (!base::Contains(TestFixture::usages(),
                      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
    GTEST_SKIP();
  }

  for (auto format : TestFixture::formats()) {
#if BUILDFLAG(IS_OZONE)
    if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
        !ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
            viz::SharedImageFormatToBufferFormat(format),
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
      continue;
    }
#endif

    gfx::GpuMemoryBufferHandle handle;
    TestFixture::CreateGpuMemoryBuffer(
        kBufferSize, format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        &handle);
    if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
      continue;
    }

    std::unique_ptr<MappableBuffer> buffer(
        TestFixture::CreateMappableBufferFromHandle(
            std::move(handle), kBufferSize, format,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE));
    ASSERT_TRUE(buffer);

    // Map buffer into user space.
    ASSERT_TRUE(buffer->Map());

    // Copy and compare mapped buffers.
    int num_planes = format.NumberOfPlanes();
    for (int plane = 0; plane < num_planes; ++plane) {
      const size_t row_size_in_bytes =
          viz::SharedMemoryRowSizeForSharedImageFormat(format, plane,
                                                       kBufferSize.width())
              .value_or(0);
      EXPECT_GT(row_size_in_bytes, 0u);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      UNSAFE_TODO(memset(data.data(), 0x2a + plane, row_size_in_bytes));

      size_t height = format.GetPlaneSize(plane, kBufferSize).height();
      for (size_t y = 0; y < height; ++y) {
        UNSAFE_TODO(memcpy(static_cast<char*>(buffer->memory(plane)) +
                               y * buffer->stride(plane),
                           data.data(), row_size_in_bytes));
        UNSAFE_TODO(
            EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                    y * buffer->stride(plane),
                                data.data(), row_size_in_bytes)));
      }
    }

    buffer->Unmap();

    // Remap the buffer, and compare again. It should contain the same data.
    ASSERT_TRUE(buffer->Map());

    for (int plane = 0; plane < num_planes; ++plane) {
      const size_t row_size_in_bytes =
          viz::SharedMemoryRowSizeForSharedImageFormat(format, plane,
                                                       kBufferSize.width())
              .value_or(0);
      EXPECT_GT(row_size_in_bytes, 0u);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      UNSAFE_TODO(memset(data.data(), 0x2a + plane, row_size_in_bytes));

      size_t height = format.GetPlaneSize(plane, kBufferSize).height();
      for (size_t y = 0; y < height; ++y) {
        UNSAFE_TODO(
            EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                    y * buffer->stride(plane),
                                data.data(), row_size_in_bytes)));
      }
    }

    buffer->Unmap();
  }
}
#endif

TYPED_TEST_P(MappableBufferTest, SerializeAndDeserialize) {
  const gfx::Size kBufferSize(8, 8);
  const gfx::GpuMemoryBufferType kBufferType = TypeParam::kBufferType;

  for (auto format : TestFixture::formats()) {
    for (auto usage : TestFixture::usages()) {
#if BUILDFLAG(IS_OZONE)
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
              viz::SharedImageFormatToBufferFormat(format), usage)) {
        continue;
      }
#endif

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);
      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle output_handle;
      mojo::test::SerializeAndDeserialize<gfx::mojom::GpuMemoryBufferHandle>(
          handle, output_handle);
      EXPECT_EQ(output_handle.type, kBufferType);

      std::unique_ptr<MappableBuffer> buffer(
          TestFixture::CreateMappableBufferFromHandle(
              std::move(output_handle), kBufferSize, format, usage));
      ASSERT_TRUE(buffer);
    }
  }
}

// The MappableBufferTest test case verifies behavior that is expected
// from a GpuMemoryBuffer implementation in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P(MappableBufferTest,
                            CreateFromHandle,
#if !BUILDFLAG(IS_ANDROID)
                            CreateFromHandleSmallBuffer,
                            Map,
                            PersistentMap,
#endif
                            SerializeAndDeserialize);
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_INTERNAL_MAPPABLE_BUFFER_TEST_TEMPLATE_H_
