// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_utils.h"
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
    display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::StrictMock<::gl::MockGLInterface>>();
    ::gl::MockGLInterface::SetGLInterface(gl_.get());
    switch (GetParam()) {
      case kMockedAndroid: {
        test_values_.gpu.vendor_id = 0;  // not implemented
        test_values_.gpu.device_id = 0;  // not implemented
        test_values_.gpu.driver_vendor = "";  // not implemented
        test_values_.gpu.driver_version = "14.0";
        test_values_.gpu.pixel_shader_version = "1.00";
        test_values_.gpu.vertex_shader_version = "1.00";
        test_values_.gpu.gl_renderer = "Adreno (TM) 320";
        test_values_.gpu.gl_vendor = "Qualcomm";
        test_values_.gpu.gl_version = "OpenGL ES 2.0 V@14.0 AU@04.02 (CL@3206)";
        test_values_.gpu.gl_extensions =
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
        test_values_.gpu.pixel_shader_version = "1.50";
        test_values_.gpu.vertex_shader_version = "1.50";
        test_values_.gpu.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gpu.gl_vendor = "NVIDIA Corporation";
        test_values_.gpu.gl_version = "3.2.0 NVIDIA 195.36.24";
        test_values_.gpu.gl_extensions =
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
        test_values_.gpu.pixel_shader_version = "1.20";
        test_values_.gpu.vertex_shader_version = "1.20";
        test_values_.gpu.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
        test_values_.gpu.gl_vendor = "NVIDIA Corporation";
        test_values_.gpu.gl_version = "2.1 NVIDIA-1.6.18";
        test_values_.gpu.gl_extensions =
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
        test_values_.gpu.pixel_shader_version = "1.40";
        test_values_.gpu.vertex_shader_version = "1.40";
        test_values_.gpu.gl_renderer = "Quadro FX 380/PCI/SSE2";
        test_values_.gpu.gl_vendor = "NVIDIA Corporation";
        test_values_.gpu.gl_version = "3.1.0";
        test_values_.gpu.gl_extensions =
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
    context_->SetExtensionsString(test_values_.gpu.gl_extensions.c_str());
    context_->SetGLVersionString(test_values_.gpu.gl_version.c_str());
    context_->SetGLDisplayEGL(display_->GetAs<gl::GLDisplayEGL>());
    surface_ = new gl::GLSurfaceStub;
    context_->MakeCurrent(surface_.get());

    EXPECT_CALL(*gl_, GetString(GL_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gpu.gl_version.c_str())));

    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gpu.gl_renderer.c_str())));

    // Now that that expectation is set up, we can call this helper function.
    if (gl::WillUseGLGetStringForExtensions()) {
      EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
          .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
              test_values_.gpu.gl_extensions.c_str())));
    } else {
      split_extensions_.clear();
      split_extensions_ =
          base::SplitString(test_values_.gpu.gl_extensions, " ",
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
            test_values_.gpu.gl_vendor.c_str())));
    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            test_values_.gpu.gl_renderer.c_str())));
    EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_SAMPLES, _))
        .WillOnce(SetArgPointee<1>(8))
        .RetiresOnSaturation();
  }

  void TearDown() override {
    ::gl::MockGLInterface::SetGLInterface(nullptr);
    gl_.reset();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);

    testing::Test::TearDown();
  }

 protected:
  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  GPUInfo test_values_;
  scoped_refptr<gl::GLContextStub> context_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  const char* gl_shading_language_version_ = nullptr;

  // Persistent storage is needed for the split extension string.
  std::vector<std::string> split_extensions_;

  raw_ptr<gl::GLDisplay> display_ = nullptr;
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
  gpu_info.gpu.system_device_id = display_->system_device_id();
  CollectGraphicsInfoGL(&gpu_info, display_);
#if BUILDFLAG(IS_WIN)
  if (GetParam() == kMockedWindows) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    // Skip testing the driver version on Windows because it's
    // obtained from the bot's registry.
  }
#elif BUILDFLAG(IS_MAC)
  if (GetParam() == kMockedMacOSX) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#elif BUILDFLAG(IS_ANDROID)
  if (GetParam() == kMockedAndroid) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#else  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (GetParam() == kMockedLinux) {
    EXPECT_EQ(test_values_.gpu.driver_vendor, gpu_info.gpu.driver_vendor);
    EXPECT_EQ(test_values_.gpu.driver_version, gpu_info.gpu.driver_version);
  }
#endif

  EXPECT_EQ(test_values_.gpu.pixel_shader_version,
            gpu_info.gpu.pixel_shader_version);
  EXPECT_EQ(test_values_.gpu.vertex_shader_version,
            gpu_info.gpu.vertex_shader_version);
  EXPECT_EQ(test_values_.gpu.gl_version, gpu_info.gpu.gl_version);
  EXPECT_EQ(test_values_.gpu.gl_renderer, gpu_info.gpu.gl_renderer);
  EXPECT_EQ(test_values_.gpu.gl_vendor, gpu_info.gpu.gl_vendor);
  EXPECT_EQ(test_values_.gpu.gl_extensions, gpu_info.gpu.gl_extensions);
}

class MultiGPUsTest
    : public testing::Test,
      public ::testing::WithParamInterface<MockedOperatingSystemKind> {
 public:
  MultiGPUsTest() = default;
  ~MultiGPUsTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
    display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<::testing::StrictMock<::gl::MockGLInterface>>();
    ::gl::MockGLInterface::SetGLInterface(gl_.get());
    // Need to make a context current so that WillUseGLGetStringForExtensions
    // can be called
    context_ = new gl::GLContextStub;
    context_->SetGLDisplayEGL(display_->GetAs<gl::GLDisplayEGL>());
    surface_ = new gl::GLSurfaceStub;
    context_->MakeCurrent(surface_.get());
  }

  void TearDown() override {
    ::gl::MockGLInterface::SetGLInterface(nullptr);
    gl_.reset();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);

    testing::Test::TearDown();
  }

 protected:
  // Use StrictMock to make 100% sure we know how GL will be called.
  std::unique_ptr<::testing::StrictMock<::gl::MockGLInterface>> gl_;
  scoped_refptr<gl::GLContextStub> context_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(MultiGPUsTest);

TEST_F(MultiGPUsTest, IdentifyActiveGPU) {
  GPUDevice nvidia_gpu;
  nvidia_gpu.system_device_id = 0x10de;
  GPUDevice intel_gpu;
  intel_gpu.system_device_id = 0x8086;

  GPUInfo gpu_info;
  gpu_info.gpu = nvidia_gpu;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);

  gpu_info.secondary_gpus[0].system_device_id = display_->system_device_id();
  IdentifyActiveGPU(&gpu_info);
  EXPECT_FALSE(gpu_info.gpu.active);
  EXPECT_TRUE(gpu_info.secondary_gpus[0].active);

  gpu_info.secondary_gpus[0].system_device_id = 0x8086;
  gpu_info.gpu.system_device_id = display_->system_device_id();
  IdentifyActiveGPU(&gpu_info);
  EXPECT_TRUE(gpu_info.gpu.active);
  EXPECT_FALSE(gpu_info.secondary_gpus[0].active);
}

}  // namespace gpu
