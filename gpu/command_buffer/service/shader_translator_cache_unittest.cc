// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {

TEST(ShaderTranslatorCacheTest, InitParamComparable) {
  // Tests that ShaderTranslatorInitParams padding or padding of its
  // members does not affect the object equality or ordering.

  ShBuiltInResources a_resources;
  memset(&a_resources, 88, sizeof(a_resources));
  sh::InitBuiltInResources(&a_resources);

  ShBuiltInResources b_resources;
  memset(&b_resources, 77, sizeof(b_resources));
  sh::InitBuiltInResources(&b_resources);

  EXPECT_TRUE(memcmp(&a_resources, &b_resources, sizeof(a_resources)) == 0);

  ShCompileOptions driver_bug_workarounds{};

  char a_storage[sizeof(ShaderTranslatorCache::ShaderTranslatorInitParams)];
  memset(a_storage, 55, sizeof(a_storage));
  ShaderTranslatorCache::ShaderTranslatorInitParams* a =
      new (&a_storage) ShaderTranslatorCache::ShaderTranslatorInitParams(
          GL_VERTEX_SHADER, SH_GLES2_SPEC, a_resources, SH_ESSL_OUTPUT,
          driver_bug_workarounds);

  ShaderTranslatorCache::ShaderTranslatorInitParams b(
      GL_VERTEX_SHADER, SH_GLES2_SPEC, b_resources, SH_ESSL_OUTPUT,
      driver_bug_workarounds);

  EXPECT_TRUE(*a == b);
  EXPECT_FALSE(*a < b || b < *a);

  memset(a_storage, 55, sizeof(a_storage));
  a = new (&a_storage) ShaderTranslatorCache::ShaderTranslatorInitParams(b);

  EXPECT_TRUE(*a == b);
  EXPECT_FALSE(*a < b || b < *a);
}
}  // namespace gles2
}  // namespace gpu
