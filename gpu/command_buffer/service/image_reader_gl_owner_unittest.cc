// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_owner.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/android/android_image_reader_compat.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/image_reader_gl_owner.h"
#include "gpu/command_buffer/service/mock_abstract_texture.h"
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
    if (!IsImageReaderSupported())
      return;

    gl::init::InitializeStaticGLBindingsImplementation(
        gl::kGLImplementationEGLGLES2, false);
    gl::init::InitializeGLOneOffPlatformImplementation(false, false, true);

    surface_ = new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240));
    surface_->Initialize();

    share_group_ = new gl::GLShareGroup();
    context_ = new gl::GLContextEGL(share_group_.get());
    context_->Initialize(surface_.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context_->MakeCurrent(surface_.get()));

    // Create a texture.
    glGenTextures(1, &texture_id_);

    std::unique_ptr<MockAbstractTexture> texture =
        std::make_unique<MockAbstractTexture>(texture_id_);
    abstract_texture_ = texture->AsWeakPtr();
    image_reader_ = TextureOwner::Create(std::move(texture), SecureMode());
  }

  virtual TextureOwner::Mode SecureMode() {
    return TextureOwner::Mode::kAImageReaderInsecure;
  }

  void TearDown() override {
    if (texture_id_ && context_->MakeCurrent(surface_.get()))
      glDeleteTextures(1, &texture_id_);
    image_reader_ = nullptr;
    context_ = nullptr;
    share_group_ = nullptr;
    surface_ = nullptr;
    gl::init::ShutdownGL(false);
  }

  bool IsImageReaderSupported() const {
    return base::android::AndroidImageReader::GetInstance().IsSupported();
  }

  scoped_refptr<TextureOwner> image_reader_;
  GLuint texture_id_ = 0;

  base::WeakPtr<MockAbstractTexture> abstract_texture_;

  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLSurface> surface_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ImageReaderGLOwnerTest, ImageReaderObjectCreation) {
  if (!IsImageReaderSupported())
    return;

  ASSERT_TRUE(image_reader_);
}

TEST_F(ImageReaderGLOwnerTest, ScopedJavaSurfaceCreation) {
  if (!IsImageReaderSupported())
    return;

  gl::ScopedJavaSurface temp = image_reader_->CreateJavaSurface();
  ASSERT_TRUE(temp.IsValid());
}

// Verify that ImageReaderGLOwner creates a bindable GL texture, and deletes
// it during destruction.
TEST_F(ImageReaderGLOwnerTest, GLTextureIsCreatedAndDestroyed) {
  if (!IsImageReaderSupported())
    return;

  // |texture_id| should not work anymore after we delete image_reader_.
  image_reader_ = nullptr;
  EXPECT_FALSE(abstract_texture_);
}

// Make sure that image_reader_ remembers the correct context and surface.
TEST_F(ImageReaderGLOwnerTest, ContextAndSurfaceAreCaptured) {
  if (!IsImageReaderSupported())
    return;

  ASSERT_EQ(context_, image_reader_->GetContext());
  ASSERT_EQ(surface_, image_reader_->GetSurface());
}

// Verify that destruction works even if some other context is current.
TEST_F(ImageReaderGLOwnerTest, DestructionWorksWithWrongContext) {
  if (!IsImageReaderSupported())
    return;

  scoped_refptr<gl::GLSurface> new_surface(
      new gl::PbufferGLSurfaceEGL(gfx::Size(320, 240)));
  new_surface->Initialize();

  scoped_refptr<gl::GLShareGroup> new_share_group(new gl::GLShareGroup());
  scoped_refptr<gl::GLContext> new_context(
      new gl::GLContextEGL(new_share_group.get()));
  new_context->Initialize(new_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(new_context->MakeCurrent(new_surface.get()));

  image_reader_ = nullptr;
  EXPECT_FALSE(abstract_texture_);

  // |new_context| should still be current.
  ASSERT_TRUE(new_context->IsCurrent(new_surface.get()));

  new_context = nullptr;
  new_share_group = nullptr;
  new_surface = nullptr;
}

// The max number of images used by the ImageReader must be 2 for non-Surface
// control except for certain devices for which it is limited to 1.
TEST_F(ImageReaderGLOwnerTest, MaxImageExpectation) {
  if (!IsImageReaderSupported())
    return;
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            base::android::AndroidImageReader::LimitAImageReaderMaxSizeToOne()
                ? 1
                : 2);
}

class ImageReaderGLOwnerSecureSurfaceControlTest
    : public ImageReaderGLOwnerTest {
 public:
  TextureOwner::Mode SecureMode() final {
    return TextureOwner::Mode::kAImageReaderSecureSurfaceControl;
  }
};

TEST_F(ImageReaderGLOwnerSecureSurfaceControlTest, CreatesSecureAImageReader) {
  if (!IsImageReaderSupported())
    return;

  ASSERT_TRUE(image_reader_);
  auto* a_image_reader = static_cast<ImageReaderGLOwner*>(image_reader_.get())
                             ->image_reader_for_testing();
  int32_t format = AIMAGE_FORMAT_YUV_420_888;
  base::android::AndroidImageReader::GetInstance().AImageReader_getFormat(
      a_image_reader, &format);
  EXPECT_EQ(format, AIMAGE_FORMAT_PRIVATE);
}

// The max number of images used by the ImageReader must be 3 for Surface
// control.
TEST_F(ImageReaderGLOwnerSecureSurfaceControlTest, MaxImageExpectation) {
  if (!IsImageReaderSupported())
    return;
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            3);
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
  if (!IsImageReaderSupported())
    return;
  EXPECT_EQ(static_cast<ImageReaderGLOwner*>(image_reader_.get())
                ->max_images_for_testing(),
            3);
}

}  // namespace gpu
