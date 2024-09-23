// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/transform_feedback_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::_;

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

}  // namespace gles2
}  // namespace gpu
