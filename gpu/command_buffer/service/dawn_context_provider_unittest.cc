// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_context_provider.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"  // nogncheck

namespace gpu {
namespace {

class DawnContextProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    dawnProcSetProcs(&dawn::native::GetProcs());
    features::InitSkiaGraphiteDefaultParamsForTesting();

    context_provider_ = DawnContextProvider::CreateWithBackend(
        wgpu::BackendType::Vulkan, /*force_fallback_adapter=*/true,
        GpuPreferences(), GpuFeatureInfo());
    if (!context_provider_) {
      GTEST_SKIP() << "DawnContextProvider creation failed";
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DawnContextProvider> context_provider_;
};

TEST_F(DawnContextProviderTest, ErrorExitCallbackInvokedOnError) {
  base::test::TestFuture<std::string> exit_future;
  context_provider_->SetErrorExitCallback(base::BindRepeating(
      [](base::RepeatingCallback<void(std::string)> cb,
         std::string_view reason) { cb.Run(std::string(reason)); },
      exit_future.GetRepeatingCallback()));

  context_provider_->GetDevice().InjectError(wgpu::ErrorType::OutOfMemory,
                                             "simulated OOM");
  EXPECT_THAT(exit_future.Get(),
              testing::HasSubstr(
                  base::NumberToString(static_cast<int>(error::kOutOfMemory))));
  EXPECT_EQ(context_provider_->GetResetStatus(), error::kOutOfMemory);
}

}  // namespace
}  // namespace gpu
