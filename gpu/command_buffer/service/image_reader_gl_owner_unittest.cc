// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_reader_gl_owner.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

class ImageReaderGLOwnerTest : public testing::Test {
 public:
  ImageReaderGLOwnerTest() {}
  ~ImageReaderGLOwnerTest() override {}

 protected:
  void SetUp() override {
    gl::init::InitializeStaticGLBindingsImplementation(
        gl::GLImplementationParts(gl::kGLImplementationEGLGLES2));
    display_ = gl::init::InitializeGLOneOffPlatformImplementation(
        /*disable_gl_drawing=*/false,
        /*init_extensions=*/true,
        /*gpu_preference=*/gl::GpuPreference::kDefault);

    scoped_refptr<gl::GLSurface> surface(new gl::PbufferGLSurfaceEGL(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size(320, 240)));
    surface->Initialize();

    share_group_ = new gl::GLShareGroup();
    context_ = new gl::GLContextEGL(share_group_.get());
    context_->Initialize(surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_->default_surface());
    ASSERT_TRUE(context_->MakeCurrentDefault());

    GpuDriverBugWorkarounds workarounds;
    auto context_state = base::MakeRefCounted<SharedContextState>(
        share_group_, surface, context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing(),
        GrContextType::kGL);
    context_state->InitializeSkia(GpuPreferences(), workarounds);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state->InitializeGL(GpuPreferences(), std::move(feature_info));

    image_reader_ = new ImageReaderGLOwner(
        SecureMode(), std::move(context_state),
        features::NeedThreadSafeAndroidMedia()
            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
            : nullptr,
        TextureOwnerCodecType::kMediaCodec);
  }

  virtual TextureOwner::Mode SecureMode() {
    return TextureOwner::Mode::kAImageReaderInsecure;
  }

  void TearDown() override {
    if (texture_id_ && context_->MakeCurrentDefault()) {
      glDeleteTextures(1, &texture_id_);
    }
    image_reader_ = nullptr;
    context_ = nullptr;
    share_group_ = nullptr;
    gl::init::ShutdownGL(display_, false);
  }

  scoped_refptr<TextureOwner> image_reader_;
  GLuint texture_id_ = 0;

  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

TEST_F(ImageReaderGLOwnerTest, ImageReaderObjectCreation) {
  ASSERT_TRUE(image_reader_);
}

TEST_F(ImageReaderGLOwnerTest, ScopedJavaSurfaceCreation) {
  gl::ScopedJavaSurface temp = image_reader_->CreateJavaSurface();
  ASSERT_TRUE(temp.IsValid());
}


// Verify that destruction works even if some other context is current.
TEST_F(ImageReaderGLOwnerTest, DestructionWorksWithWrongContext) {
  scoped_refptr<gl::GLSurface> new_surface(new gl::PbufferGLSurfaceEGL(
      gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size(320, 240)));
  new_surface->Initialize();

  scoped_refptr<gl::GLShareGroup> new_share_group(new gl::GLShareGroup());
  scoped_refptr<gl::GLContext> new_context(
      new gl::GLContextEGL(new_share_group.get()));
  new_context->Initialize(new_surface.get(), gl::GLContextAttribs());
  new_surface = nullptr;
  ASSERT_TRUE(new_context->MakeCurrentDefault());

  image_reader_ = nullptr;

  // |new_context| should still be current.
  ASSERT_TRUE(new_context->IsCurrent(new_context->default_surface()));

  new_context = nullptr;
  new_share_group = nullptr;
}

// The max number of images used by the ImageReader must be 2 for non-Surface
// control except for certain devices for which it is limited to 1.
TEST_F(ImageReaderGLOwnerTest, MaxImageExpectation) {
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            features::LimitAImageReaderMaxSizeToOne() ? 1 : 2);
}

class ImageReaderGLOwnerSecureSurfaceControlTest
    : public ImageReaderGLOwnerTest {
 public:
  TextureOwner::Mode SecureMode() final {
    return TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
  }
};

TEST_F(ImageReaderGLOwnerSecureSurfaceControlTest, CreatesSecureAImageReader) {
  ASSERT_TRUE(image_reader_);
  auto* a_image_reader = static_cast<ImageReaderGLOwner*>(image_reader_.get())
                             ->image_reader_for_testing();
  int32_t format = AIMAGE_FORMAT_YUV_420_888;
  AImageReader_getFormat(a_image_reader, &format);
  EXPECT_EQ(format, AIMAGE_FORMAT_PRIVATE);
}

// The max number of images used by the ImageReader must be 3 for Surface
// control.
TEST_F(ImageReaderGLOwnerSecureSurfaceControlTest, MaxImageExpectation) {
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            features::IncreaseBufferCountForHighFrameRate() ? 5 : 3);
}

class ImageReaderGLOwnerInsecureSurfaceControlTest
    : public ImageReaderGLOwnerTest {
 public:
  TextureOwner::Mode SecureMode() final {
    return TextureOwner::Mode::kAImageReaderInsecureSurfaceControl;
  }
};

// The max number of images used by the ImageReader must be 3 for Surface
// control.
TEST_F(ImageReaderGLOwnerInsecureSurfaceControlTest, MaxImageExpectation) {
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            features::IncreaseBufferCountForHighFrameRate() ? 5 : 3);
}

}  // namespace gpu
