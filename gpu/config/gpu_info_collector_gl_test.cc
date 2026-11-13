// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "gpu/config/gpu_info_collector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

#if BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)
TEST(GPUConfig, CollectGraphicsInfoDawnGLES) {
  EGLContext contextBefore = eglGetCurrentContext();
  base::HistogramTester histograms;

  GpuPreferences gpu_preferences;
  std::vector<std::string> dawn_info_list;
  CollectDawnInfo(gpu_preferences, true, &dawn_info_list);
  EXPECT_EQ(contextBefore, eglGetCurrentContext());
#if BUILDFLAG(USE_DAWN)
  histograms.ExpectTotalCount("GPU.WebGPU.Support", 1);
#endif

  CollectDawnInfo(gpu_preferences, false, &dawn_info_list);
  EGLContext contextAfter = eglGetCurrentContext();
  EXPECT_TRUE(contextBefore == contextAfter);
}
#endif  // BUILDFLAG(DAWN_ENABLE_BACKEND_OPENGLES)

}  // namespace gpu
