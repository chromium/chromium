// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

TEST(AssistiveTechnologyControllerTest, EnableAndDisableFeatures) {
  AssistiveTechnologyControllerImpl at_controller;
  // Turn everything on.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    at_controller.EnableAssistiveTechnology(type, /*enabled=*/true);
    EXPECT_TRUE(at_controller.IsFeatureEnabled(type));
  }
  // Turn everything off.
  for (int i = static_cast<int>(mojom::AssistiveTechnologyType::kMinValue);
       i < static_cast<int>(mojom::AssistiveTechnologyType::kMaxValue); i++) {
    mojom::AssistiveTechnologyType type =
        static_cast<mojom::AssistiveTechnologyType>(i);
    at_controller.EnableAssistiveTechnology(type, /*enabled=*/false);
    EXPECT_FALSE(at_controller.IsFeatureEnabled(type));
  }
}

}  // namespace ax
