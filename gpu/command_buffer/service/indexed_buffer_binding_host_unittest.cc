// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/indexed_buffer_binding_host.h"
#include "ui/gl/gl_mock.h"

namespace gpu {
namespace gles2 {

namespace {
const uint32_t kMaxBindings = 16;
const GLuint kBufferClientId = 87;
const GLuint kBufferServiceId = 987;
}  // namespace anonymous

class IndexedBufferBindingHostTest : public GpuServiceTest {
 public:
  IndexedBufferBindingHostTest()
      : uniform_host_(new IndexedBufferBindingHost(kMaxBindings,
                                                   GL_UNIFORM_BUFFER,
                                                   true,
                                                   false)),
        tf_host_(new IndexedBufferBindingHost(kMaxBindings,
                                              GL_TRANSFORM_FEEDBACK_BUFFER,
                                              true,
                                              false)),
        buffer_manager_(new BufferManager(nullptr, nullptr)) {
    buffer_manager_->CreateBuffer(kBufferClientId, kBufferServiceId);
    buffer_ = buffer_manager_->GetBuffer(kBufferClientId);
    DCHECK(buffer_.get());
  }

  ~IndexedBufferBindingHostTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUpWithGLVersion("OpenGL ES 3.0", "");
  }

  void TearDown() override {
    buffer_ = nullptr;
    buffer_manager_->MarkContextLost();
    buffer_manager_->Destroy();
    buffer_manager_.reset();
    GpuServiceTest::TearDown();
  }

  void SetBufferSize(GLenum target, GLsizeiptr size) {
    buffer_manager_->SetInfo(
        buffer_.get(), target, size, GL_STATIC_DRAW, false);
  }

  scoped_refptr<IndexedBufferBindingHost> uniform_host_;
  scoped_refptr<IndexedBufferBindingHost> tf_host_;
  std::unique_ptr<BufferManager> buffer_manager_;
  scoped_refptr<Buffer> buffer_;
};

TEST_F(IndexedBufferBindingHostTest, DoBindBufferRangeUninitializedBuffer) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLuint kIndex = 2;
  const GLintptr kOffset = 4;
  const GLsizeiptr kSize = 8;

  EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, kBufferServiceId))
      .Times(1)
      .RetiresOnSaturation();

  tf_host_->DoBindBufferRange(kIndex, buffer_.get(), kOffset, kSize);

  for (uint32_t index = 0; index < kMaxBindings; ++index) {
    if (index != kIndex) {
      EXPECT_EQ(nullptr, tf_host_->GetBufferBinding(index));
    } else {
      EXPECT_EQ(buffer_.get(), tf_host_->GetBufferBinding(index));
      EXPECT_EQ(kSize, tf_host_->GetBufferSize(index));
      EXPECT_EQ(kOffset, tf_host_->GetBufferStart(index));
    }
  }

  tf_host_->RemoveBoundBuffer(kTarget, buffer_.get(), nullptr, false);
}

TEST_F(IndexedBufferBindingHostTest, DoBindBufferRangeBufferWithoutEnoughSize) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLuint kIndex = 2;
  const GLintptr kOffset = 4;
  const GLsizeiptr kSize = 8;
  const GLsizeiptr kBufferSize = kOffset + kSize - 2;

  SetBufferSize(kTarget, kBufferSize);

  GLsizeiptr clamped_size = ((kBufferSize - kOffset) >> 2) << 2;

  EXPECT_CALL(*gl_, BindBufferRange(kTarget, kIndex, kBufferServiceId, kOffset,
                                    clamped_size))
      .Times(1)
      .RetiresOnSaturation();

  tf_host_->DoBindBufferRange(kIndex, buffer_.get(), kOffset, kSize);

  for (uint32_t index = 0; index < kMaxBindings; ++index) {
    if (index != kIndex) {
      EXPECT_EQ(nullptr, tf_host_->GetBufferBinding(index));
    } else {
      EXPECT_EQ(buffer_.get(), tf_host_->GetBufferBinding(index));
      EXPECT_EQ(kSize, tf_host_->GetBufferSize(index));
      EXPECT_EQ(kOffset, tf_host_->GetBufferStart(index));
    }
  }

  // Now adjust buffer size to be big enough.
  EXPECT_CALL(*gl_, BindBufferRange(kTarget, kIndex, kBufferServiceId, kOffset,
                                    kSize))
      .Times(1)
      .RetiresOnSaturation();

  SetBufferSize(kTarget, kOffset + kSize);
  tf_host_->OnBufferData(buffer_.get());

  tf_host_->RemoveBoundBuffer(kTarget, buffer_.get(), nullptr, false);
}

TEST_F(IndexedBufferBindingHostTest, RestoreBindings) {
  const GLenum kTarget = GL_UNIFORM_BUFFER;
  const GLuint kIndex = 2;
  const GLuint kOtherIndex = 10;
  const GLintptr kOffset = 4;
  const GLsizeiptr kSize = 8;
  const GLsizeiptr kBufferSize = kOffset + kSize - 2;

  GLsizeiptr clamped_size = ((kBufferSize - kOffset) >> 2) << 2;

  SetBufferSize(kTarget, kBufferSize);
  // Set up host
  EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, kBufferServiceId))
      .Times(1)
      .RetiresOnSaturation();
  uniform_host_->DoBindBufferBase(kIndex, buffer_.get());
  // Set up the second host
  scoped_refptr<IndexedBufferBindingHost> other =
      new IndexedBufferBindingHost(kMaxBindings, kTarget, true, false);
  EXPECT_CALL(*gl_, BindBufferRange(kTarget, kOtherIndex, kBufferServiceId,
                                    kOffset, clamped_size))
      .Times(1)
      .RetiresOnSaturation();
  other->DoBindBufferRange(kOtherIndex, buffer_.get(), kOffset, kSize);

  {
    // Switching from |other| to |uniform_host_|.
    EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, kBufferServiceId))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindBufferBase(kTarget, kOtherIndex, 0))
        .Times(1)
        .RetiresOnSaturation();
    uniform_host_->RestoreBindings(other.get());
  }

  {
    // Switching from |uniform_host_| to |other|.
    EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, 0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindBufferRange(kTarget, kOtherIndex, kBufferServiceId,
                                      kOffset, clamped_size))
        .Times(1)
        .RetiresOnSaturation();
    other->RestoreBindings(uniform_host_.get());
  }

  EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, 0))
      .Times(1)
      .RetiresOnSaturation();
  uniform_host_->RemoveBoundBuffer(kTarget, buffer_.get(), nullptr, true);
}

}  // namespace gles2
}  // namespace gpu
