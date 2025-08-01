// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file defines tests that implementations of GpuMemoryBufferImpl should
// pass in order to be conformant.

#ifndef GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_
#define GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
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
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "gpu/ipc/common/gpu_memory_buffer_impl_io_surface.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "gpu/ipc/common/gpu_memory_buffer_impl_native_pixmap.h"
#include "ui/ozone/public/client_native_pixmap_factory_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#endif

namespace gpu {

template <typename GpuMemoryBufferImplType>
class GpuMemoryBufferImplTest : public testing::Test {
 public:
  GpuMemoryBufferImplTest() {
#if BUILDFLAG(IS_OZONE)
    client_native_pixmap_factory_ = ui::CreateClientNativePixmapFactoryOzone();
#endif
  }

  void CreateGpuMemoryBuffer(const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             gfx::GpuMemoryBufferHandle* handle) {
    GpuMemoryBufferImplType::AllocateForTesting(size, format, usage, handle);
  }

  std::unique_ptr<GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) {
    switch (handle.type) {
      case gfx::SHARED_MEMORY_BUFFER:
        return GpuMemoryBufferImplSharedMemory::CreateFromHandleForTesting(
            std::move(handle), size, format, usage);
#if BUILDFLAG(IS_MAC)
      case gfx::IO_SURFACE_BUFFER:
        return GpuMemoryBufferImplIOSurface::CreateFromHandleForTesting(
            std::move(handle), size, format, usage);
#endif
#if BUILDFLAG(IS_OZONE)
      case gfx::NATIVE_PIXMAP:
        return GpuMemoryBufferImplNativePixmap::CreateFromHandleForTesting(
            client_native_pixmap_factory_.get(), std::move(handle), size,
            format, usage);
#endif
#if BUILDFLAG(IS_WIN)
      case gfx::DXGI_SHARED_HANDLE:
        return GpuMemoryBufferImplDXGI::CreateFromHandleForTesting(
            std::move(handle), size, format);
#endif
      default:
        NOTREACHED();
    }
  }

  GpuMemoryBufferSupport* gpu_memory_buffer_support() {
    return &gpu_memory_buffer_support_;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)
  // Overridden from testing::Test:
  void SetUp() override {
    // https://crrev.com/c/5348599
    // GmbImplTestNativePixmap is a no-op, we should run it on a gpu runner.
#if BUILDFLAG(IS_OZONE)
    // TODO(329211602): Currently only wayland has a valid
    // IsNativeGpuMemoryBufferConfigurationSupportedForTesting. We should
    // implement that in X11 and other platforms either.
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

 private:
  bool run_gpu_test_ = false;
  GpuMemoryBufferSupport gpu_memory_buffer_support_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
#if BUILDFLAG(IS_OZONE)
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;
#endif
};

// Subclass test case for tests that require a Create() method,
// not all implementations have that.
template <typename GpuMemoryBufferImplType>
class GpuMemoryBufferImplCreateTest : public testing::Test {
 public:
  GpuMemoryBufferImplCreateTest() = default;

  GpuMemoryBufferSupport* gpu_memory_buffer_support() {
    return &gpu_memory_buffer_support_;
  }

 private:
  GpuMemoryBufferSupport gpu_memory_buffer_support_;
};

TYPED_TEST_SUITE_P(GpuMemoryBufferImplTest);

TYPED_TEST_P(GpuMemoryBufferImplTest, CreateFromHandle) {
  const gfx::Size kBufferSize(8, 8);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE,
        gfx::BufferUsage::PROTECTED_SCANOUT,
        gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
    };
    for (auto usage : usages) {
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !TestFixture::gpu_memory_buffer_support()
               ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
                   format, usage)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);

      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      std::unique_ptr<GpuMemoryBufferImpl> buffer(
          TestFixture::CreateGpuMemoryBufferImplFromHandle(
              std::move(handle), kBufferSize, format, usage));
      ASSERT_TRUE(buffer);
    }
  }
}

TYPED_TEST_P(GpuMemoryBufferImplTest, CreateFromHandleSmallBuffer) {
  const gfx::Size kBufferSize(8, 8);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE,
        gfx::BufferUsage::PROTECTED_SCANOUT,
        gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
    };
    for (auto usage : usages) {
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !TestFixture::gpu_memory_buffer_support()
               ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
                   format, usage)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);
      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      gfx::Size bogus_size = kBufferSize;
      bogus_size.Enlarge(100, 100);

      // Handle import should fail when the size is bigger than expected.
      std::unique_ptr<GpuMemoryBufferImpl> buffer(
          TestFixture::CreateGpuMemoryBufferImplFromHandle(
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

TYPED_TEST_P(GpuMemoryBufferImplTest, Map) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  const gfx::Size kBufferSize(4, 4);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
        !TestFixture::gpu_memory_buffer_support()
             ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
                 format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
      continue;
    }

    gfx::GpuMemoryBufferHandle handle;
    TestFixture::CreateGpuMemoryBuffer(
        kBufferSize, format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        &handle);
    if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
      continue;
    }

    std::unique_ptr<GpuMemoryBufferImpl> buffer(
        TestFixture::CreateGpuMemoryBufferImplFromHandle(
            std::move(handle), kBufferSize, format,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE));
    ASSERT_TRUE(buffer);

    const size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);

    // Map buffer into user space.
    ASSERT_TRUE(buffer->Map());

    // Map the buffer a second time. This should be a noop and simply allow
    // multiple clients concurrent read access. Likewise a subsequent Unmap()
    // shouldn't invalidate the first's Map().
    ASSERT_TRUE(buffer->Map());
    buffer->Unmap();

    // Copy and compare mapped buffers.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      const size_t row_size_in_bytes =
          gfx::RowSizeForBufferFormat(kBufferSize.width(), format, plane);
      EXPECT_GT(row_size_in_bytes, 0u);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      memset(data.data(), 0x2a + plane, row_size_in_bytes);

      size_t height = kBufferSize.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
      for (size_t y = 0; y < height; ++y) {
        memcpy(static_cast<char*>(buffer->memory(plane)) +
                   y * buffer->stride(plane),
               data.data(), row_size_in_bytes);
        EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                y * buffer->stride(plane),
                            data.data(), row_size_in_bytes));
      }
    }

    buffer->Unmap();
  }
}

TYPED_TEST_P(GpuMemoryBufferImplTest, PersistentMap) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  const gfx::Size kBufferSize(4, 4);

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
        !TestFixture::gpu_memory_buffer_support()
             ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
                 format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE)) {
      continue;
    }

    gfx::GpuMemoryBufferHandle handle;
    TestFixture::CreateGpuMemoryBuffer(
        kBufferSize, format, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        &handle);
    if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
      continue;
    }

    std::unique_ptr<GpuMemoryBufferImpl> buffer(
        TestFixture::CreateGpuMemoryBufferImplFromHandle(
            std::move(handle), kBufferSize, format,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE));
    ASSERT_TRUE(buffer);

    // Map buffer into user space.
    ASSERT_TRUE(buffer->Map());

    // Copy and compare mapped buffers.
    size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
    for (size_t plane = 0; plane < num_planes; ++plane) {
      const size_t row_size_in_bytes =
          gfx::RowSizeForBufferFormat(kBufferSize.width(), format, plane);
      EXPECT_GT(row_size_in_bytes, 0u);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      memset(data.data(), 0x2a + plane, row_size_in_bytes);

      size_t height = kBufferSize.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
      for (size_t y = 0; y < height; ++y) {
        memcpy(static_cast<char*>(buffer->memory(plane)) +
                   y * buffer->stride(plane),
               data.data(), row_size_in_bytes);
        EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                y * buffer->stride(plane),
                            data.data(), row_size_in_bytes));
      }
    }

    buffer->Unmap();

    // Remap the buffer, and compare again. It should contain the same data.
    ASSERT_TRUE(buffer->Map());

    for (size_t plane = 0; plane < num_planes; ++plane) {
      const size_t row_size_in_bytes =
          gfx::RowSizeForBufferFormat(kBufferSize.width(), format, plane);

      auto data = base::HeapArray<char>::Uninit(row_size_in_bytes);
      memset(data.data(), 0x2a + plane, row_size_in_bytes);

      size_t height = kBufferSize.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
      for (size_t y = 0; y < height; ++y) {
        EXPECT_EQ(0, memcmp(static_cast<char*>(buffer->memory(plane)) +
                                y * buffer->stride(plane),
                            data.data(), row_size_in_bytes));
      }
    }

    buffer->Unmap();
  }
}

TYPED_TEST_P(GpuMemoryBufferImplTest, SerializeAndDeserialize) {
  const gfx::Size kBufferSize(8, 8);
  const gfx::GpuMemoryBufferType kBufferType = TypeParam::kBufferType;

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    gfx::BufferUsage usages[] = {
        gfx::BufferUsage::GPU_READ,
        gfx::BufferUsage::SCANOUT,
        gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
        gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VDA_WRITE,
        gfx::BufferUsage::PROTECTED_SCANOUT,
        gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE,
        gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
        gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
    };
    for (auto usage : usages) {
      if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
          !TestFixture::gpu_memory_buffer_support()
               ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(
                   format, usage)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle handle;
      TestFixture::CreateGpuMemoryBuffer(kBufferSize, format, usage, &handle);
      if (!TestFixture::CheckGpuMemoryBufferHandle(handle)) {
        continue;
      }

      gfx::GpuMemoryBufferHandle output_handle;
      mojo::test::SerializeAndDeserialize<gfx::mojom::GpuMemoryBufferHandle>(
          handle, output_handle);
      EXPECT_EQ(output_handle.type, kBufferType);

      std::unique_ptr<GpuMemoryBufferImpl> buffer(
          TestFixture::CreateGpuMemoryBufferImplFromHandle(
              std::move(output_handle), kBufferSize, format, usage));
      ASSERT_TRUE(buffer);
    }
  }
}

// The GpuMemoryBufferImplTest test case verifies behavior that is expected
// from a GpuMemoryBuffer implementation in order to be conformant.
REGISTER_TYPED_TEST_SUITE_P(GpuMemoryBufferImplTest,
                            CreateFromHandle,
                            CreateFromHandleSmallBuffer,
                            Map,
                            PersistentMap,
                            SerializeAndDeserialize);

TYPED_TEST_SUITE_P(GpuMemoryBufferImplCreateTest);

TYPED_TEST_P(GpuMemoryBufferImplCreateTest, Create) {
  const gfx::Size kBufferSize(8, 8);
  gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;

  for (auto format : gfx::GetBufferFormatsForTesting()) {
    if (TypeParam::kBufferType != gfx::SHARED_MEMORY_BUFFER &&
        !TestFixture::gpu_memory_buffer_support()
             ->IsNativeGpuMemoryBufferConfigurationSupportedForTesting(format,
                                                                       usage)) {
      continue;
    }
    std::unique_ptr<TypeParam> buffer(
        TypeParam::CreateForTesting(kBufferSize, format, usage));
    ASSERT_TRUE(buffer);
  }
}
// The GpuMemoryBufferImplCreateTest test case verifies behavior that is
// expected from a GpuMemoryBuffer Create() implementation in order to be
// conformant.
REGISTER_TYPED_TEST_SUITE_P(GpuMemoryBufferImplCreateTest, Create);

}  // namespace gpu

#endif  // GPU_IPC_COMMON_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_
