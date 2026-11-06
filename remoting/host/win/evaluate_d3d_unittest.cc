// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/evaluate_d3d.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(EvaluateD3DTest, DirectXCapturerSupported) {
  std::vector<std::string> capabilities = {"DirectX-Capturer", "MinD3DGE10",
                                           "MinD3DGE11"};
  EXPECT_TRUE(HasDirectXCapturerCapability(capabilities));
}

TEST(EvaluateD3DTest, NoDirectXCapturer) {
  std::vector<std::string> capabilities = {"No-DirectX-Capturer", "MinD3DGE10",
                                           "MinD3DGE11"};
  EXPECT_FALSE(HasDirectXCapturerCapability(capabilities));
}

TEST(EvaluateD3DTest, MissingDirectXCapturerToken) {
  EXPECT_FALSE(HasDirectXCapturerCapability({"MinD3DLT10"}));
  EXPECT_FALSE(HasDirectXCapturerCapability({}));
}

}  // namespace remoting
