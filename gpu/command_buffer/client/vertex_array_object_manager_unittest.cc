// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/vertex_array_object_manager.h"

#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class VertexArrayObjectManagerTest : public testing::Test {
 protected:
  static const GLuint kMaxAttribs = 4;

  void SetUp() override {
    manager_ = std::make_unique<VertexArrayObjectManager>(kMaxAttribs);
  }
  void TearDown() override {}

  std::unique_ptr<VertexArrayObjectManager> manager_;
};

const GLuint VertexArrayObjectManagerTest::kMaxAttribs;

TEST_F(VertexArrayObjectManagerTest, Basic) {
  // Check out of bounds access.
  uint32_t param;
  void* ptr;
  EXPECT_FALSE(manager_->GetVertexAttrib(
      kMaxAttribs, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &param));
  EXPECT_FALSE(manager_->GetAttribPointer(
      kMaxAttribs, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr));
  // Check defaults.
  for (GLuint ii = 0; ii < kMaxAttribs; ++ii) {
    EXPECT_TRUE(manager_->GetVertexAttrib(
        ii, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param));
    EXPECT_EQ(0u, param);
    EXPECT_TRUE(manager_->GetVertexAttrib(
        ii, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &param));
    EXPECT_EQ(0u, param);
    EXPECT_TRUE(manager_->GetVertexAttrib(
        ii, GL_VERTEX_ATTRIB_ARRAY_SIZE, &param));
    EXPECT_EQ(4u, param);
    EXPECT_TRUE(manager_->GetVertexAttrib(
        ii, GL_VERTEX_ATTRIB_ARRAY_TYPE, &param));
    EXPECT_EQ(static_cast<uint32_t>(GL_FLOAT), param);
    EXPECT_TRUE(manager_->GetVertexAttrib(
        ii, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &param));
    EXPECT_EQ(0u, param);
    EXPECT_TRUE(manager_->GetAttribPointer(
        ii, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr));
    EXPECT_TRUE(nullptr == ptr);
  }
}

TEST_F(VertexArrayObjectManagerTest, UnbindBuffer) {
  const GLuint kBufferToUnbind = 123;
  const GLuint kBufferToRemain = 456;
  const GLuint kElementArray = 789;
  bool changed = false;
  GLuint ids[2] = { 1, 3, };
  manager_->GenVertexArrays(std::size(ids), ids);
  // Bind buffers to attribs on 2 vaos.
  for (size_t ii = 0; ii < std::size(ids); ++ii) {
    UNSAFE_TODO(EXPECT_TRUE(manager_->BindVertexArray(ids[ii], &changed)));
    EXPECT_TRUE(manager_->SetAttribPointer(
        kBufferToUnbind, 0, 4, GL_FLOAT, false, 0, 0, GL_FALSE));
    EXPECT_TRUE(manager_->SetAttribPointer(
        kBufferToRemain, 1, 4, GL_FLOAT, false, 0, 0, GL_FALSE));
    EXPECT_TRUE(manager_->SetAttribPointer(
        kBufferToUnbind, 2, 4, GL_FLOAT, false, 0, 0, GL_FALSE));
    EXPECT_TRUE(manager_->SetAttribPointer(
        kBufferToRemain, 3, 4, GL_FLOAT, false, 0, 0, GL_FALSE));
    for (size_t jj = 0; jj < 4u; ++jj) {
      manager_->SetAttribEnable(jj, true);
    }
    manager_->BindElementArray(kElementArray);
  }
  EXPECT_TRUE(manager_->BindVertexArray(ids[0], &changed));

  // Unbind the buffer.
  manager_->UnbindBuffer(kBufferToUnbind);
  manager_->UnbindBuffer(kElementArray);

  // Check the status of the bindings.
  static const auto expected = std::to_array<std::array<const uint32_t, 4>>({
      {
          0,
          kBufferToRemain,
          0,
          kBufferToRemain,
      },
      {
          kBufferToUnbind,
          kBufferToRemain,
          kBufferToUnbind,
          kBufferToRemain,
      },
  });
  static const auto expected_element_array = std::to_array<GLuint>({
      0,
      kElementArray,
  });
  for (size_t ii = 0; ii < std::size(ids); ++ii) {
    UNSAFE_TODO(EXPECT_TRUE(manager_->BindVertexArray(ids[ii], &changed)));
    for (size_t jj = 0; jj < 4; ++jj) {
      uint32_t param = 1;
      EXPECT_TRUE(manager_->GetVertexAttrib(
          jj, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param));
      EXPECT_EQ(expected[ii][jj], param)
          << "id: " << UNSAFE_TODO(ids[ii]) << ", attrib: " << jj;
    }
    EXPECT_EQ(expected_element_array[ii],
              manager_->bound_element_array_buffer());
  }
}

TEST_F(VertexArrayObjectManagerTest, GetSet) {
  const char* dummy = "dummy";
  const void* p = reinterpret_cast<const void*>(dummy);
  manager_->SetAttribEnable(1, true);
  manager_->SetAttribPointer(123, 1, 3, GL_BYTE, true, 3, p, GL_TRUE);
  uint32_t param;
  void* ptr;
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &param));
  EXPECT_EQ(123u, param);
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &param));
  EXPECT_NE(0u, param);
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &param));
  EXPECT_EQ(3u, param);
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_TYPE, &param));
  EXPECT_EQ(static_cast<uint32_t>(GL_BYTE), param);
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &param));
  EXPECT_NE(0u, param);
  EXPECT_TRUE(manager_->GetVertexAttrib(
      1, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &param));
  EXPECT_EQ(1u, param);
  EXPECT_TRUE(manager_->GetAttribPointer(
      1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr));
  EXPECT_EQ(p, ptr);

  // Check that getting the divisor is passed to the service.
  // This is because the divisor is an optional feature which
  // only the service can validate.
  EXPECT_FALSE(manager_->GetVertexAttrib(
      0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE, &param));
}


TEST_F(VertexArrayObjectManagerTest, BindElementArray) {
  bool changed = false;
  GLuint ids[2] = { 1, 3, };
  manager_->GenVertexArrays(std::size(ids), ids);

  // Check the default element array is 0.
  EXPECT_EQ(0u, manager_->bound_element_array_buffer());
  // Check binding the same array does not need a service call.
  EXPECT_FALSE(manager_->BindElementArray(0u));
  // Check binding a new element array requires a service call.
  EXPECT_TRUE(manager_->BindElementArray(55u));
  // Check the element array was bound.
  EXPECT_EQ(55u, manager_->bound_element_array_buffer());
  // Check binding the same array does not need a service call.
  EXPECT_FALSE(manager_->BindElementArray(55u));

  // Check with a new vao.
  EXPECT_TRUE(manager_->BindVertexArray(1, &changed));
  // Check the default element array is 0.
  EXPECT_EQ(0u, manager_->bound_element_array_buffer());
  // Check binding a new element array requires a service call.
  EXPECT_TRUE(manager_->BindElementArray(11u));
  // Check the element array was bound.
  EXPECT_EQ(11u, manager_->bound_element_array_buffer());
  // Check binding the same array does not need a service call.
  EXPECT_FALSE(manager_->BindElementArray(11u));

  // check switching vao bindings returns the correct element array.
  EXPECT_TRUE(manager_->BindVertexArray(3, &changed));
  EXPECT_EQ(0u, manager_->bound_element_array_buffer());
  EXPECT_TRUE(manager_->BindVertexArray(0, &changed));
  EXPECT_EQ(55u, manager_->bound_element_array_buffer());
  EXPECT_TRUE(manager_->BindVertexArray(1, &changed));
  EXPECT_EQ(11u, manager_->bound_element_array_buffer());
}

TEST_F(VertexArrayObjectManagerTest, GenBindDelete) {
  // Check unknown array fails.
  bool changed = false;
  EXPECT_FALSE(manager_->BindVertexArray(123, &changed));
  EXPECT_FALSE(changed);

  GLuint ids[2] = { 1, 3, };
  manager_->GenVertexArrays(std::size(ids), ids);
  // Check Genned arrays succeed.
  EXPECT_TRUE(manager_->BindVertexArray(1, &changed));
  EXPECT_TRUE(changed);
  EXPECT_TRUE(manager_->BindVertexArray(3, &changed));
  EXPECT_TRUE(changed);

  // Check binding the same array returns changed as false.
  EXPECT_TRUE(manager_->BindVertexArray(3, &changed));
  EXPECT_FALSE(changed);

  // Check deleted ararys fail to bind
  manager_->DeleteVertexArrays(2, ids);
  EXPECT_FALSE(manager_->BindVertexArray(1, &changed));
  EXPECT_FALSE(changed);
  EXPECT_FALSE(manager_->BindVertexArray(3, &changed));
  EXPECT_FALSE(changed);

  // Check binding 0 returns changed as false since it's
  // already bound.
  EXPECT_TRUE(manager_->BindVertexArray(0, &changed));
  EXPECT_FALSE(changed);
}

}  // namespace gles2
}  // namespace gpu
