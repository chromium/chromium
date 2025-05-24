// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_core_options.h"

namespace blink {

TEST(ResolveSamplingParamsOptionTest, NoOptions) {
  auto result = ResolveSamplingParamsOption(nullptr);
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value());
}

TEST(ResolveSamplingParamsOptionTest, NoSamplingParams) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value());
}

TEST(ResolveSamplingParamsOptionTest, ValidOptions) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(1);
  options->setTemperature(0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->top_k, 1u);
  ASSERT_EQ(result.value()->temperature, 0.5);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTopK(10);
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_TRUE(result2.has_value());
  ASSERT_EQ(result2.value()->top_k, 10u);
  ASSERT_EQ(result2.value()->temperature, 0.5);

  auto* options3 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options3->setTopK(10);
  options3->setTemperature(0);
  auto result3 = ResolveSamplingParamsOption(options3);
  ASSERT_TRUE(result3.has_value());
  ASSERT_EQ(result3.value()->top_k, 10u);
  ASSERT_EQ(result3.value()->temperature, 0);
}

TEST(ResolveSamplingParamsOptionTest, OnlyOneOfTopKAndTemperatureIsProvided) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(10);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(),
            SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_FALSE(result2.has_value());
  ASSERT_EQ(result2.error(),
            SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided);
}

TEST(ResolveSamplingParamsOptionTest, InvalidTopK) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(0.5);
  options->setTemperature(0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), SamplingParamsOptionError::kInvalidTopK);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTopK(-1);
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_FALSE(result2.has_value());
  ASSERT_EQ(result2.error(), SamplingParamsOptionError::kInvalidTopK);
}

TEST(ResolveSamplingParamsOptionTest, InvalidTemperature) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(10);
  options->setTemperature(-0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), SamplingParamsOptionError::kInvalidTemperature);
}

}  // namespace blink
