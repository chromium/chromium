// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transform_feedback_manager.h"

#include <memory>

#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace gpu {
namespace gles2 {

namespace {
const GLuint kTransformFeedbackClientId = 76;
const GLuint kTransformFeedbackServiceId = 1076;
}  // anonymous

class TransformFeedbackManagerTest : public GpuServiceTest {
 public:
  TransformFeedbackManagerTest() = default;

  ~TransformFeedbackManagerTest() override = default;

 protected:
  void SetUp() override {
    const GLuint kMaxTransformFeedbackSeparateAttribs = 16;
    GpuServiceTest::SetUpWithGLVersion("OpenGL ES 3.0", "");
    manager_ = std::make_unique<TransformFeedbackManager>(
        kMaxTransformFeedbackSeparateAttribs, true);
  }

  void TearDown() override {
    manager_.reset();
    GpuServiceTest::TearDown();
  }

  std::unique_ptr<TransformFeedbackManager> manager_;
};

TEST_F(TransformFeedbackManagerTest, LifeTime) {
  manager_->CreateTransformFeedback(
      kTransformFeedbackClientId, kTransformFeedbackServiceId);
  scoped_refptr<TransformFeedback> transform_feedback =
      manager_->GetTransformFeedback(kTransformFeedbackClientId);
  EXPECT_TRUE(transform_feedback.get());

  manager_->RemoveTransformFeedback(kTransformFeedbackClientId);
  EXPECT_FALSE(manager_->GetTransformFeedback(kTransformFeedbackClientId));

  EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, _))
      .Times(1)
      .RetiresOnSaturation();
  transform_feedback = nullptr;
}

TEST_F(TransformFeedbackManagerTest, BufferBindingTrackedWhilePaused) {
  const GLuint kBufferClientId = 11;
  const GLuint kBufferServiceId = 1011;
  const GLuint kClientId2 = 77;
  const GLuint kServiceId2 = 1077;

  EXPECT_CALL(*gl_, BindTransformFeedback(_, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BindBufferBase(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BeginTransformFeedback(_)).Times(AnyNumber());
  EXPECT_CALL(*gl_, PauseTransformFeedback()).Times(AnyNumber());
  EXPECT_CALL(*gl_, EndTransformFeedback()).Times(AnyNumber());
  EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, _)).Times(AnyNumber());

  BufferManager buffer_manager(nullptr, nullptr);
  buffer_manager.CreateBuffer(kBufferClientId, kBufferServiceId);
  scoped_refptr<Buffer> buffer = buffer_manager.GetBuffer(kBufferClientId);
  ASSERT_TRUE(buffer.get());

  scoped_refptr<TransformFeedback> tf1 = manager_->CreateTransformFeedback(
      kTransformFeedbackClientId, kTransformFeedbackServiceId);
  scoped_refptr<TransformFeedback> tf2 =
      manager_->CreateTransformFeedback(kClientId2, kServiceId2);

  tf1->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, nullptr, nullptr);
  tf1->DoBindBufferBase(0, buffer.get());
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  tf1->DoBeginTransformFeedback(GL_POINTS);
  tf1->DoPauseTransformFeedback();
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  // Switching to another transform feedback object while the previous one is
  // still active (paused) must not drop the buffer's transform feedback
  // binding state, otherwise it could be re-specified before it is resumed.
  tf2->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf1.get(), nullptr);
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  tf1->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf2.get(), nullptr);
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());
  EXPECT_FALSE(buffer->IsDoubleBoundForTransformFeedback());

  tf1->DoEndTransformFeedback();
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  tf2->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf1.get(), nullptr);
  EXPECT_FALSE(buffer->IsBoundForTransformFeedback());

  manager_->Destroy();
  tf1 = nullptr;
  tf2 = nullptr;
  buffer = nullptr;
  buffer_manager.MarkContextLost();
  buffer_manager.Destroy();
}

TEST_F(TransformFeedbackManagerTest,
       BufferBindingTrackedWhenActiveTfDestroyedUnbound) {
  const GLuint kBufferClientId = 11;
  const GLuint kBufferServiceId = 1011;
  const GLuint kClientId2 = 77;
  const GLuint kServiceId2 = 1077;

  // Expect glDeleteTransformFeedbacks when destroying the unbound active TF
  // tf1. Importantly, we do NOT expect glEndTransformFeedback to be called on
  // tf1 since it was unbound when destroyed.
  EXPECT_CALL(*gl_, BindTransformFeedback(_, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BindBuffer(_, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BindBufferBase(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*gl_, BeginTransformFeedback(_)).Times(AnyNumber());
  EXPECT_CALL(*gl_, PauseTransformFeedback()).Times(AnyNumber());
  EXPECT_CALL(*gl_, DeleteTransformFeedbacks(1, _)).Times(AnyNumber());
  // glEndTransformFeedback must not be called!
  EXPECT_CALL(*gl_, EndTransformFeedback()).Times(0);

  BufferManager buffer_manager(nullptr, nullptr);
  buffer_manager.CreateBuffer(kBufferClientId, kBufferServiceId);
  scoped_refptr<Buffer> buffer = buffer_manager.GetBuffer(kBufferClientId);
  ASSERT_TRUE(buffer.get());

  scoped_refptr<TransformFeedback> tf1 = manager_->CreateTransformFeedback(
      kTransformFeedbackClientId, kTransformFeedbackServiceId);
  scoped_refptr<TransformFeedback> tf2 =
      manager_->CreateTransformFeedback(kClientId2, kServiceId2);

  tf1->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, nullptr, nullptr);
  tf1->DoBindBufferBase(0, buffer.get());
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  tf1->DoBeginTransformFeedback(GL_POINTS);
  tf1->DoPauseTransformFeedback();
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  // Bind to tf2. Now tf1 is unbound but remains active (paused).
  tf2->DoBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf1.get(), nullptr);
  EXPECT_TRUE(buffer->IsBoundForTransformFeedback());

  // Destroy tf1 while active and unbound.
  // This must unbind the buffers attached to it (OnUnbind should be called),
  // reducing transform_feedback_indexed_binding_count_ to 0.
  // In addition, glEndTransformFeedback must not be called in the driver.
  manager_->RemoveTransformFeedback(kTransformFeedbackClientId);
  tf1 = nullptr;
  EXPECT_FALSE(buffer->IsBoundForTransformFeedback());

  manager_->Destroy();
  tf2 = nullptr;
  buffer = nullptr;
  buffer_manager.MarkContextLost();
  buffer_manager.Destroy();
}

}  // namespace gles2
}  // namespace gpu
