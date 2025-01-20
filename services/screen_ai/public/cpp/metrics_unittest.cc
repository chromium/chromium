// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/public/cpp/metrics.h"

#include "base/metrics/metrics_hashes.h"
#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

namespace {

TEST(ScreenAIMetricsTest, NoLanguages) {
  chrome_screen_ai::VisualAnnotation annotation;
  annotation.add_lines();

  std::optional<uint64_t> most_detected_language =
      GetMostDetectedLanguageInOcrData(annotation);
  EXPECT_FALSE(most_detected_language.has_value());
}

TEST(ScreenAIMetricsTest, BaseAndLocaleAreDifferent) {
  chrome_screen_ai::VisualAnnotation annotation;
  auto* line = annotation.add_lines();
  line->add_words()->set_language("en-US");

  std::optional<uint64_t> most_detected_language =
      GetMostDetectedLanguageInOcrData(annotation);
  EXPECT_TRUE(most_detected_language.has_value());
  EXPECT_EQ(most_detected_language.value(), base::HashMetricName("en-US"));
}

TEST(ScreenAIMetricsTest, BaseAndLocaleAreSame) {
  chrome_screen_ai::VisualAnnotation annotation;
  auto* line = annotation.add_lines();
  line->add_words()->set_language("ru-RU");

  std::optional<uint64_t> most_detected_language =
      GetMostDetectedLanguageInOcrData(annotation);
  EXPECT_TRUE(most_detected_language.has_value());
  EXPECT_EQ(most_detected_language.value(), base::HashMetricName("ru"));
}

TEST(ScreenAIMetricsTest, RecordOnlyMostDetectedLanguage) {
  chrome_screen_ai::VisualAnnotation annotation;
  auto* line = annotation.add_lines();
  line->add_words()->set_language("en-US");
  line->add_words()->set_language("es-US");
  line->add_words()->set_language("fr-CA");

  std::optional<uint64_t> most_detected_language =
      GetMostDetectedLanguageInOcrData(annotation);
  EXPECT_TRUE(most_detected_language.has_value());
  EXPECT_EQ(most_detected_language.value(), base::HashMetricName("en-US"));
}

}  // namespace

}  // namespace screen_ai
