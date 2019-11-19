// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::gl::MockGLInterface;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace {

// Allows testing of all configurations on all operating systems.
enum MockedOperatingSystemKind {
  kMockedAndroid,
  kMockedLinux,
  kMockedMacOSX,
  kMockedWindows
};

}  // anonymous namespace

namespace gpu {

static const MockedOperatingSystemKind kMockedOperatingSystemKinds[] = {
  kMockedAndroid,
  kMockedLinux,
  kMockedMacOSX,
  kMockedWindows
};

class GPUInfoCollectorTest
    : public testing::Test,
      public ::testing::WithParamInterface<MockedOperatingSystemKind> {
 public:
  GPUInfoCollectorTest() = default;
  ~GPUInfoCollectorTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
    gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_.reset(new ::testing::StrictMock<::gl::MockGLInterface>());
    ::gl::MockGLInterface::SetGLInterface(gl_.get());
    switch (GetParam()) {
      case kMockedAndroid: {
        test_values_.gpu.vendor_id = 0;  // not implemented
        test_values_.gpu.device_id = 0;  // not implemented
        test_values_.gpu.driver_vendor = "";  // not implemented
        test_values_.gpu.driver_version = "14.0";
        test_values_.pixel_shader_version = "1.00";
        test_values_.vertex_shader_version = "1.00";
        test_values_.gl_renderer = "Adreno (TM) 320";
        test_values_.gl_vendor = "Qualcomm";
        test_values_.gl_version = "OpenGL ES 2.0 V@14.0 AU@04.02 (CL@3206)";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra GL_EXT_multisampled_render_to_texture";
        gl_shading_language_version_ = "1.00";
        break;
      }
      case kMockedLinux: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0658;
        test_values_.gpu.driver_vendor = "NVIDIA";
        test_values_.gpu.driver_version = "195.36.24";
        test_values_.pixel_shader_version = "1.50";
        test_values_.vertex_shader_version = "1.50";
        test_values_.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "3.2.0 NVIDIA 195.36.24";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra";
        gl_shading_language_version_ = "1.50 NVIDIA via Cg compiler";
        break;
      }
      case kMockedMacOSX: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0640;
        test_values_.gpu.driver_vendor = "NVIDIA";
        test_values_.gpu.driver_version = "1.6.18";
        test_values_.pixel_shader_version = "1.20";
        test_values_.vertex_shader_version = "1.20";
        test_values_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "2.1 NVIDIA-1.6.18";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra GL_EXT_framebuffer_multisample";
        gl_shading_language_version_ = "1.20 ";
        break;
      }
      case kMockedWindows: {
        test_values_.gpu.vendor_id = 0x10de;
        test_values_.gpu.device_id = 0x0658;
        test_values_.gpu.driver_vendor = "";  // not implemented
        test_values_.gpu.driver_version = "";
        test_values_.pixel_shader_version = "1.40";
        test_values_.vertex_shader_version = "1.40";
        test_values_.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gl_vendor = "NVIDIA Corporation";
        test_values_.gl_version = "3.1.0";
        test_values_.gl_extensions =
            "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
            "GL_EXT_read_format_bgra";
        gl_shading_language_version_ = "1.40 NVIDIA via Cg compiler";
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }

    // Need to make a context current so that WillUseGLGetStringForExtensions
    // can be called
    context_ = new gl::GLContextStub;
    context_->SetExtensionsString(test_values_.gl_extensions.c_str());
    context_->SetGLVersionString(test_values_.gl_version.c_str());
    surface_ = new gl::GLSurfaceStub;
    context_->MakeCurrent(surface_.get());

    EXPECT_CALL(*gl_, GetString(GL_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_version.c_str())));

    // Now that that expectation is set up, we can call this helper function.
    if (gl::WillUseGLGetStringForExtensions()) {
      EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
          .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
              test_values_.gl_extensions.c_str())));
    } else {
      split_extensions_.clear();
      split_extensions_ = base::SplitString(
          test_values_.gl_extensions, " ",
          base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      EXPECT_CALL(*gl_, GetIntegerv(GL_NUM_EXTENSIONS, _))
          .WillRepeatedly(SetArgPointee<1>(split_extensions_.size()));
      for (size_t ii = 0; ii < split_extensions_.size(); ++ii) {
        EXPECT_CALL(*gl_, GetStringi(GL_EXTENSIONS, ii))
            .WillRepeatedly(Return(reinterpret_cast<const uint8_t*>(
                split_extensions_[ii].c_str())));
      }
    }
    EXPECT_CALL(*gl_, GetString(GL_SHADING_LANGUAGE_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_shading_language_version_)));
    EXPECT_CALL(*gl_, GetString(GL_VENDOR))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_vendor.c_str())));
    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gl_renderer.c_str())));
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_SAMPLES, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  void TearDown() override {
    ::gl::MockGLInterface::SetGLInterface(nullptr);
    gl_.reset();
    gl::init::ShutdownGL(false);

    testing::Test::TearDown();
  }

 public:
  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  GPUInfo test_values_;
  scoped_refptr<gl::GLContextStub> context_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  const char* gl_shading_language_version_ = nullptr;

  // Persistent storage is needed for the split extension string.
  std::vector<std::string> split_extensions_;
};

INSTANTIATE_TEST_SUITE_P(GPUConfig,
                         GPUInfoCollectorTest,
                         ::testing::ValuesIn(kMockedOperatingSystemKinds));

// TODO(rlp): Test the vendor and device id collection if deemed necessary as
//            it involves several complicated mocks for each platform.

// TODO(kbr): This test still has platform-dependent behavior because
// CollectDriverInfoGL behaves differently per platform. This should
// be fixed.
TEST_P(GPUInfoCollectorTest, CollectGraphicsInfoGL) {
  GPUInfo gpu_info;
  CollectGraphicsInfoGL(&gpu_info);
#if defined(OS_WIN)
  if (GetParam() == kMockedWindows) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    // Skip testing the driver version on Windows because it's
    // obtained from the bot's registry.
  }
#elif defined(OS_MACOSX)
  if (GetParam() == kMockedMacOSX) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#elif defined(OS_ANDROID)
  if (GetParam() == kMockedAndroid) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#else  // defined (OS_LINUX)
  if (GetParam() == kMockedLinux) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#endif

  EXPECT_EQ(test_values_.pixel_shader_version,
            gpu_info.pixel_shader_version);
  EXPECT_EQ(test_values_.vertex_shader_version,
            gpu_info.vertex_shader_version);
  EXPECT_EQ(test_values_.gl_version, gpu_info.gl_version);
  EXPECT_EQ(test_values_.gl_renderer, gpu_info.gl_renderer);
  EXPECT_EQ(test_values_.gl_vendor, gpu_info.gl_vendor);
  EXPECT_EQ(test_values_.gl_extensions, gpu_info.gl_extensions);
}

TEST(MultiGPUsTest, IdentifyActiveGPU0) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x0416;

  GPUInfo gpu_info;
  gpu_info.gpu = nvidia_gpu;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "Intel Open Source Technology Center";
  gpu_info.gl_renderer = "Mesa DRI Intel(R) Haswell Mobile";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_TRUE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "NVIDIA Corporation";
  gpu_info.gl_renderer = "Quadro 600/PCIe/SSE2";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

TEST(MultiGPUsTest, IdentifyActiveGPU1) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0de1;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x040a;

  GPUInfo gpu_info;
  gpu_info.gpu = intel_gpu;
  gpu_info.secondary_gpus.push_back(nvidia_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "nouveau";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_TRUE(gpu_info.secondary_gpus[0].active);
}

TEST(MultiGPUsTest, IdentifyActiveGPU2) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0de1;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x040a;

  GPUInfo gpu_info;
  gpu_info.gpu = intel_gpu;
  gpu_info.secondary_gpus.push_back(nvidia_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  gpu_info.gl_vendor = "Intel";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

TEST(MultiGPUsTest, IdentifyActiveGPU3) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0de1;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x040a;
  GPUInfo::GPUDevice amd_gpu;
  amd_gpu.vendor_id = 0x1002;
  amd_gpu.device_id = 0x6779;

  GPUInfo gpu_info;
  gpu_info.gpu = intel_gpu;
  gpu_info.secondary_gpus.push_back(nvidia_gpu);
  gpu_info.secondary_gpus.push_back(amd_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
  EXPECT_FALSE(gpu_info.secondary_gpus[1].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
  EXPECT_FALSE(gpu_info.secondary_gpus[1].active);

  gpu_info.gl_vendor = "X.Org";
  gpu_info.gl_renderer = "AMD R600";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
  EXPECT_TRUE(gpu_info.secondary_gpus[1].active);
}

TEST(MultiGPUsTest, IdentifyActiveGPU4) {
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0de1;

  GPUInfo gpu_info;
  gpu_info.gpu = nvidia_gpu;

  EXPECT_FALSE(gpu_info.gpu.active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);

  gpu_info.gl_vendor = "nouveau";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);
}

TEST(MultiGPUsTest, IdentifyActiveGPUAvoidFalseMatch) {
  // Verify that "Corporation" won't be matched with "ati".
  GPUInfo::GPUDevice amd_gpu;
  amd_gpu.vendor_id = 0x1002;
  amd_gpu.device_id = 0x0df8;
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x0416;

  GPUInfo gpu_info;
  gpu_info.gpu = amd_gpu;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  gpu_info.gl_vendor = "Google Corporation";
  gpu_info.gl_renderer = "Chrome GPU Team";
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

}  // namespace gpu
