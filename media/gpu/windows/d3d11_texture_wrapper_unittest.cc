// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_texture_wrapper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/gpu/test/fake_command_buffer_helper.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::Values;

namespace media {

class D3D11TextureWrapperUnittest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = task_environment_.GetMainThreadTaskRunner();

    display_ = gl::GLSurfaceTestSupport::InitializeOneOffImplementation(
        gl::GLImplementationParts(gl::ANGLEImplementation::kD3D11));
    surface_ = gl::init::CreateOffscreenGLSurface(display_, gfx::Size());
    share_group_ = new gl::GLShareGroup();
    context_ = gl::init::CreateGLContext(share_group_.get(), surface_.get(),
                                         gl::GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    // Create some objects that most tests want.
    fake_command_buffer_helper_ =
        base::MakeRefCounted<FakeCommandBufferHelper>(task_runner_);
    get_helper_cb_ = base::BindRepeating(
        [](scoped_refptr<CommandBufferHelper> helper) { return helper; },
        fake_command_buffer_helper_);
  }

  void TearDown() override {
    context_->ReleaseCurrent(surface_.get());
    context_ = nullptr;
    share_group_ = nullptr;
    surface_ = nullptr;
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> context_;

  // Made-up size for the images.
  const gfx::Size size_{100, 200};
  // Made-up color space for the images.
  const gfx::ColorSpace color_space_ = gfx::ColorSpace::CreateSRGB();

  // CommandBufferHelper, and a callback that returns it.  Useful to initialize
  // a wrapper.
  scoped_refptr<FakeCommandBufferHelper> fake_command_buffer_helper_;
  GetCommandBufferHelperCB get_helper_cb_;

  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

TEST_F(D3D11TextureWrapperUnittest, NV12InitSucceeds) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_NV12;

  auto wrapper = std::make_unique<DefaultTexture2DWrapper>(size_, color_space_,
                                                           dxgi_format,
                                                           /*device=*/nullptr);
  const D3D11Status init_result = wrapper->Init(
      task_runner_, get_helper_cb_, /*in_texture=*/nullptr,
      /*array_slice=*/0, /*picture_buffer=*/nullptr,
      /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing());
  EXPECT_EQ(init_result.code(), D3D11Status::Codes::kOk);

  // TODO: verify that ProcessTexture processes both textures.
}

TEST_F(D3D11TextureWrapperUnittest, BGRA8InitSucceeds) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;

  auto wrapper = std::make_unique<DefaultTexture2DWrapper>(size_, color_space_,
                                                           dxgi_format,
                                                           /*device=*/nullptr);
  const D3D11Status init_result = wrapper->Init(
      task_runner_, get_helper_cb_, /*in_texture=*/nullptr,
      /*array_slice=*/0, /*picture_buffer=*/nullptr,
      /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing());
  EXPECT_EQ(init_result.code(), D3D11Status::Codes::kOk);
}

TEST_F(D3D11TextureWrapperUnittest, FP16InitSucceeds) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;

  auto wrapper = std::make_unique<DefaultTexture2DWrapper>(size_, color_space_,
                                                           dxgi_format,
                                                           /*device=*/nullptr);
  const D3D11Status init_result = wrapper->Init(
      task_runner_, get_helper_cb_, /*in_texture=*/nullptr,
      /*array_slice=*/0, /*picture_buffer=*/nullptr,
      /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing());
  EXPECT_EQ(init_result.code(), D3D11Status::Codes::kOk);
}

TEST_F(D3D11TextureWrapperUnittest, P010InitSucceeds) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_P010;

  auto wrapper = std::make_unique<DefaultTexture2DWrapper>(size_, color_space_,
                                                           dxgi_format,
                                                           /*device=*/nullptr);
  const D3D11Status init_result = wrapper->Init(
      task_runner_, get_helper_cb_, /*in_texture=*/nullptr,
      /*array_slice=*/0, /*picture_buffer=*/nullptr,
      /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing());
  EXPECT_EQ(init_result.code(), D3D11Status::Codes::kOk);
}

TEST_F(D3D11TextureWrapperUnittest, UnknownInitFails) {
  const DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;

  auto wrapper = std::make_unique<DefaultTexture2DWrapper>(size_, color_space_,
                                                           dxgi_format,
                                                           /*device=*/nullptr);
  const D3D11Status init_result = wrapper->Init(
      task_runner_, get_helper_cb_, /*in_texture=*/nullptr,
      /*array_slice=*/0, /*picture_buffer=*/nullptr,
      /*picture_buffer_gpu_resource_init_done_cb=*/base::DoNothing());
  EXPECT_NE(init_result.code(), D3D11Status::Codes::kOk);
}

}  // namespace media
