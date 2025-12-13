// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_state.h"

#include <stddef.h>

#include <array>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

TEST(ContextStateVec4Test, DefaultValues) {
  Vec4 v;
  EXPECT_EQ(SHADER_VARIABLE_FLOAT, v.type());
  GLfloat f[4];
  v.GetValues(f);
  EXPECT_EQ(0.f, f[0]);
  EXPECT_EQ(0.f, f[1]);
  EXPECT_EQ(0.f, f[2]);
  EXPECT_EQ(1.f, f[3]);
}

TEST(ContextStateVec4Test, SetGetFloatValues) {
  Vec4 v;

  const std::array<GLfloat, 4> kFloatValues = {{2.f, 3.f, 4.f, 5.f}};
  v.SetValues(kFloatValues.data());
  EXPECT_EQ(SHADER_VARIABLE_FLOAT, v.type());
  std::array<GLfloat, 4> fv;
  v.GetValues(fv.data());
  EXPECT_EQ(kFloatValues, fv);
}

TEST(ContextStateVec4Test, SetGetIntValues) {
  Vec4 v;

  const std::array<GLint, 4> kIntValues = {{2, 3, -4, 5}};
  v.SetValues(kIntValues.data());
  EXPECT_EQ(SHADER_VARIABLE_INT, v.type());
  std::array<GLint, 4> iv;
  v.GetValues(iv.data());
  EXPECT_EQ(kIntValues, iv);
}

TEST(ContextStateVec4Test, SetGetUIntValues) {
  Vec4 v;

  const std::array<GLuint, 4> kUIntValues = {{2, 3, 4, 5}};
  v.SetValues(kUIntValues.data());
  EXPECT_EQ(SHADER_VARIABLE_UINT, v.type());
  std::array<GLuint, 4> uiv;
  v.GetValues(uiv.data());
  EXPECT_EQ(kUIntValues, uiv);
}

TEST(ContextStateVec4Test, Equal) {
  Vec4 v1, v2;

  const std::array<GLint, 4> kIntValues = {{2, 3, 4, 5}};
  const std::array<GLuint, 4> kUIntValues = {{2, 3, 4, 5}};

  v1.SetValues(kIntValues.data());
  v2.SetValues(kUIntValues.data());
  EXPECT_FALSE(v1.Equal(v2));
  EXPECT_FALSE(v2.Equal(v1));

  v2.SetValues(kIntValues.data());
  EXPECT_TRUE(v1.Equal(v2));
  EXPECT_TRUE(v2.Equal(v1));

  const std::array<GLint, 4> kIntValues2 = {{2, 3, 4, 6}};
  v2.SetValues(kIntValues2.data());
  EXPECT_FALSE(v1.Equal(v2));
  EXPECT_FALSE(v2.Equal(v1));
}

}  // namespace gles2
}  // namespace gpu


