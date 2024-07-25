// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/context_state.h"

#include <stddef.h>

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

  const GLfloat kFloatValues[4] = { 2.f, 3.f, 4.f, 5.f };
  v.SetValues(kFloatValues);
  EXPECT_EQ(SHADER_VARIABLE_FLOAT, v.type());
  GLfloat fv[4];
  v.GetValues(fv);
  for (size_t ii = 0; ii < 4; ++ii) {
    EXPECT_EQ(kFloatValues[ii], fv[ii]);
  }
}

TEST(ContextStateVec4Test, SetGetIntValues) {
  Vec4 v;

  const GLint kIntValues[4] = { 2, 3, -4, 5 };
  v.SetValues(kIntValues);
  EXPECT_EQ(SHADER_VARIABLE_INT, v.type());
  GLint iv[4];
  v.GetValues(iv);
  for (size_t ii = 0; ii < 4; ++ii) {
    EXPECT_EQ(kIntValues[ii], iv[ii]);
  }
}

TEST(ContextStateVec4Test, SetGetUIntValues) {
  Vec4 v;

  const GLuint kUIntValues[4] = { 2, 3, 4, 5 };
  v.SetValues(kUIntValues);
  EXPECT_EQ(SHADER_VARIABLE_UINT, v.type());
  GLuint uiv[4];
  v.GetValues(uiv);
  for (size_t ii = 0; ii < 4; ++ii) {
    EXPECT_EQ(kUIntValues[ii], uiv[ii]);
  }
}

TEST(ContextStateVec4Test, Equal) {
  Vec4 v1, v2;

  const GLint kIntValues[4] = { 2, 3, 4, 5 };
  const GLuint kUIntValues[4] = { 2, 3, 4, 5 };

  v1.SetValues(kIntValues);
  v2.SetValues(kUIntValues);
  EXPECT_FALSE(v1.Equal(v2));
  EXPECT_FALSE(v2.Equal(v1));

  v2.SetValues(kIntValues);
  EXPECT_TRUE(v1.Equal(v2));
  EXPECT_TRUE(v2.Equal(v1));

  const GLint kIntValues2[4] = { 2, 3, 4, 6 };
  v2.SetValues(kIntValues2);
  EXPECT_FALSE(v1.Equal(v2));
  EXPECT_FALSE(v2.Equal(v1));
}

}  // namespace gles2
}  // namespace gpu


