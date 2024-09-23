// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/public/cpp/metrics.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace screen_ai {

namespace {

constexpr char kTestMetricName[] =
    "Accessibility.PdfOcr.MostDetectedLanguageInOcrData2";

TEST(ScreenAIMetricsTest, BaseAndLocaleAreDifferent) {
  ui::AXTreeUpdate fake_tree_update;
  ui::AXNodeData fake_node_data;
  fake_node_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                    "en-US");
  fake_tree_update.nodes.emplace_back(std::move(fake_node_data));

  base::HistogramTester histogram_tester;
  RecordMostDetectedLanguageInOcrData(kTestMetricName, fake_tree_update);

  histogram_tester.ExpectTotalCount(kTestMetricName, 1);
  histogram_tester.ExpectBucketCount(kTestMetricName,
                                     base::HashMetricName("en-US"), 1);
}

TEST(ScreenAIMetricsTest, BaseAndLocaleAreSame) {
  ui::AXTreeUpdate fake_tree_update;
  ui::AXNodeData fake_node_data;
  fake_node_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                    "ru-RU");
  fake_tree_update.nodes.emplace_back(std::move(fake_node_data));

  base::HistogramTester histogram_tester;
  RecordMostDetectedLanguageInOcrData(kTestMetricName, fake_tree_update);

  histogram_tester.ExpectTotalCount(kTestMetricName, 1);
  histogram_tester.ExpectBucketCount(kTestMetricName,
                                     base::HashMetricName("ru"), 1);
}

TEST(ScreenAIMetricsTest, RecordOnlyMostDetectedLanguage) {
  ui::AXTreeUpdate fake_tree_update;
  ui::AXNodeData fake_node_us_data;
  fake_node_us_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                       "en-US");
  ui::AXNodeData fake_node_es_data;
  fake_node_es_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                       "es-US");
  ui::AXNodeData fake_node_fr_data;
  fake_node_fr_data.AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                       "fr-CA");
  fake_tree_update.nodes = {fake_node_us_data, fake_node_us_data,
                            fake_node_us_data, fake_node_es_data,
                            fake_node_es_data, fake_node_fr_data};

  base::HistogramTester histogram_tester;
  RecordMostDetectedLanguageInOcrData(kTestMetricName, fake_tree_update);

  histogram_tester.ExpectTotalCount(kTestMetricName, 1);
  histogram_tester.ExpectBucketCount(kTestMetricName,
                                     base::HashMetricName("en-US"), 1);
  histogram_tester.ExpectBucketCount(kTestMetricName,
                                     base::HashMetricName("es-US"), 0);
  histogram_tester.ExpectBucketCount(kTestMetricName,
                                     base::HashMetricName("fr-CA"), 0);
}

}  // namespace

}  // namespace screen_ai
