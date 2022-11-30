// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/multi_draw_manager.h"

#include <memory>
#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {

namespace {

using Param = std::tuple<MultiDrawManager::IndexStorageType,
                         MultiDrawManager::DrawFunction>;

}  // namespace

class MultiDrawManagerTest : public testing::TestWithParam<Param> {
 public:
  MultiDrawManagerTest()
      : multi_draw_manager_(new MultiDrawManager(std::get<0>(GetParam()))) {}

 protected:
  bool DoMultiDraw(uint32_t count,
                   GLenum mode = GL_TRIANGLES,
                   GLenum type = GL_UNSIGNED_INT) {
    std::vector<GLsizei> data(count);
    std::vector<GLint> basevertices(count);
    std::vector<GLuint> baseinstances(count);
    switch (std::get<1>(GetParam())) {
      case MultiDrawManager::DrawFunction::DrawArrays:
        return multi_draw_manager_->MultiDrawArrays(mode, data.data(),
                                                    data.data(), count);

      case MultiDrawManager::DrawFunction::DrawArraysInstanced:
        return multi_draw_manager_->MultiDrawArraysInstanced(
            mode, data.data(), data.data(), data.data(), count);

      case MultiDrawManager::DrawFunction::DrawArraysInstancedBaseInstance:
        return multi_draw_manager_->MultiDrawArraysInstancedBaseInstance(
            mode, data.data(), data.data(), data.data(), baseinstances.data(),
            count);

      case MultiDrawManager::DrawFunction::DrawElements:
        return multi_draw_manager_->MultiDrawElements(mode, data.data(), type,
                                                      data.data(), count);

      case MultiDrawManager::DrawFunction::DrawElementsInstanced:
        return multi_draw_manager_->MultiDrawElementsInstanced(
            mode, data.data(), type, data.data(), data.data(), count);

      case MultiDrawManager::DrawFunction::
          DrawElementsInstancedBaseVertexBaseInstance:
        return multi_draw_manager_
            ->MultiDrawElementsInstancedBaseVertexBaseInstance(
                mode, data.data(), type, data.data(), data.data(),
                basevertices.data(), baseinstances.data(), count);
    }
  }

  void CheckResultSize(uint32_t count,
                       const MultiDrawManager::ResultData& result) {
    MultiDrawManager::DrawFunction draw_function = std::get<1>(GetParam());
    EXPECT_TRUE(draw_function == result.draw_function);

    switch (draw_function) {
      case MultiDrawManager::DrawFunction::DrawArraysInstancedBaseInstance:
        EXPECT_TRUE(result.baseinstances.size() == count);
        [[fallthrough]];
      case MultiDrawManager::DrawFunction::DrawArraysInstanced:
        EXPECT_TRUE(result.instance_counts.size() == count);
        [[fallthrough]];
      case MultiDrawManager::DrawFunction::DrawArrays:
        EXPECT_TRUE(result.firsts.size() == count);
        EXPECT_TRUE(result.counts.size() == count);
        break;
      case MultiDrawManager::DrawFunction::
          DrawElementsInstancedBaseVertexBaseInstance:
        EXPECT_TRUE(result.basevertices.size() == count);
        EXPECT_TRUE(result.baseinstances.size() == count);
        [[fallthrough]];
      case MultiDrawManager::DrawFunction::DrawElementsInstanced:
        EXPECT_TRUE(result.instance_counts.size() == count);
        [[fallthrough]];
      case MultiDrawManager::DrawFunction::DrawElements:
        EXPECT_TRUE(result.counts.size() == count);
        switch (std::get<0>(GetParam())) {
          case MultiDrawManager::IndexStorageType::Offset:
            EXPECT_TRUE(result.offsets.size() == count);
            break;
          case MultiDrawManager::IndexStorageType::Pointer:
            EXPECT_TRUE(result.indices.size() == count);
            break;
        }
        break;
    }
  }

  std::unique_ptr<MultiDrawManager> multi_draw_manager_;
};

// Test that the simple case succeeds
TEST_P(MultiDrawManagerTest, Success) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(100));
  EXPECT_TRUE(multi_draw_manager_->End(&result));
  CheckResultSize(100, result);
}

// Test that the internal state of the multi draw manager resets such that
// successive valid multi draws can be executed
TEST_P(MultiDrawManagerTest, SuccessAfterSuccess) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(100));
  EXPECT_TRUE(multi_draw_manager_->End(&result));
  CheckResultSize(100, result);

  EXPECT_TRUE(multi_draw_manager_->Begin(1000));
  EXPECT_TRUE(DoMultiDraw(1000));
  EXPECT_TRUE(multi_draw_manager_->End(&result));
  CheckResultSize(1000, result);
}

// Test that multiple chunked multi draw calls succeed
TEST_P(MultiDrawManagerTest, SuccessMultiple) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(83));
  EXPECT_TRUE(DoMultiDraw(4));
  EXPECT_TRUE(DoMultiDraw(13));
  EXPECT_TRUE(multi_draw_manager_->End(&result));
  CheckResultSize(100, result);
}

// Test that it is invalid to submit an empty multi draw
TEST_P(MultiDrawManagerTest, Empty) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(0));
  EXPECT_FALSE(multi_draw_manager_->End(&result));
}

// Test that it is invalid to end a multi draw if it has not been started
TEST_P(MultiDrawManagerTest, EndBeforeBegin) {
  MultiDrawManager::ResultData result;
  EXPECT_FALSE(multi_draw_manager_->End(&result));
}

// Test that it is invalid to begin a multi draw twice
TEST_P(MultiDrawManagerTest, BeginAfterBegin) {
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_FALSE(multi_draw_manager_->Begin(100));
}

// Test that it is invalid to begin a multi draw twice, even if
// the first begin was empty
TEST_P(MultiDrawManagerTest, BeginAfterEmptyBegin) {
  EXPECT_TRUE(multi_draw_manager_->Begin(0));
  EXPECT_FALSE(multi_draw_manager_->Begin(100));
}

// Test that it is invalid to do a multi draw before begin
TEST_P(MultiDrawManagerTest, DrawBeforeBegin) {
  EXPECT_FALSE(DoMultiDraw(1));
}

// Test that it is invalid to end a multi draw twice
TEST_P(MultiDrawManagerTest, DoubleEnd) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(1));
  EXPECT_TRUE(DoMultiDraw(1));
  EXPECT_TRUE(multi_draw_manager_->End(&result));
  EXPECT_FALSE(multi_draw_manager_->End(&result));
}

// Test that it is invalid to end a multi draw before the drawcount
// is saturated
TEST_P(MultiDrawManagerTest, Underflow) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(99));
  EXPECT_FALSE(multi_draw_manager_->End(&result));
}

// Test that it is invalid to end a multi draw before the drawcount
// is saturated, using multiple chunks
TEST_P(MultiDrawManagerTest, UnderflowMultiple) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(42));
  EXPECT_TRUE(DoMultiDraw(31));
  EXPECT_TRUE(DoMultiDraw(26));
  EXPECT_FALSE(multi_draw_manager_->End(&result));
}

// Test that it is invalid to do a multi draw that overflows the drawcount
TEST_P(MultiDrawManagerTest, Overflow) {
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_FALSE(DoMultiDraw(101));
}

// Test that it is invalid to do a multi draw that overflows the drawcount,
// using multiple chunks
TEST_P(MultiDrawManagerTest, OverflowMultiple) {
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(31));
  EXPECT_TRUE(DoMultiDraw(49));
  EXPECT_FALSE(DoMultiDraw(21));
}

// Test that it is invalid to do a multi draw that does not match the first
// chunk's draw mode
TEST_P(MultiDrawManagerTest, DrawModeMismatch) {
  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(50, GL_TRIANGLES));
  EXPECT_FALSE(DoMultiDraw(50, GL_LINES));
}

// Test that it is invalid to do a multi draw that does not match the first
// chunk's element type
TEST_P(MultiDrawManagerTest, ElementTypeMismatch) {
  MultiDrawManager::DrawFunction draw_function = std::get<1>(GetParam());
  if (draw_function != MultiDrawManager::DrawFunction::DrawElements &&
      draw_function != MultiDrawManager::DrawFunction::DrawElementsInstanced) {
    return;
  }

  MultiDrawManager::ResultData result;
  EXPECT_TRUE(multi_draw_manager_->Begin(100));
  EXPECT_TRUE(DoMultiDraw(50, GL_TRIANGLES, GL_UNSIGNED_INT));
  EXPECT_FALSE(DoMultiDraw(50, GL_TRIANGLES, GL_UNSIGNED_SHORT));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiDrawManagerTest,
    testing::Combine(
        testing::Values(MultiDrawManager::IndexStorageType::Offset,
                        MultiDrawManager::IndexStorageType::Pointer),
        testing::Values(
            MultiDrawManager::DrawFunction::DrawArrays,
            MultiDrawManager::DrawFunction::DrawArraysInstanced,
            MultiDrawManager::DrawFunction::DrawArraysInstancedBaseInstance,
            MultiDrawManager::DrawFunction::DrawElements,
            MultiDrawManager::DrawFunction::DrawElementsInstanced,
            MultiDrawManager::DrawFunction::
                DrawElementsInstancedBaseVertexBaseInstance)));

}  // namespace gles2
}  // namespace gpu
