// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_quantizer.h"

#include <utility>
#include <vector>

#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/compute_pressure_sampler.h"
#include "content/browser/compute_pressure/compute_pressure_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

TEST(ComputePressureQuantizer, IsValid_Valid) {
  std::vector<blink::mojom::ComputePressureQuantization> valid_cases = {
      {{}, {}},
      {{0.5}, {0.5}},
      {{0.2, 0.5, 0.8}, {0.5}},
  };

  for (const auto& quantization : valid_cases) {
    EXPECT_TRUE(ComputePressureQuantizer::IsValid(quantization));
  }
}

TEST(ComputePressureQuantizer, IsValid_Invalid) {
  std::vector<blink::mojom::ComputePressureQuantization> invalid_cases = {
      {{0.2, 0.3, 0.4, 0.5}, {0.5}},  // Too many utilization thresholds.
      {{0.5}, {0.2, 0.5, 0.8}},       // Too many speed thresholds.
      {{0.2, 0.8, 0.5}, {0.5}},       // Incorrectly sorted thresholds.
      {{-1.0}, {0.5}},                // Threshold outside range.
      {{0.0}, {0.5}},
      {{1.0}, {0.5}},
      {{2.0}, {0.5}},
      {{0.5}, {-1.0}},
      {{0.5}, {0.0}},
      {{0.5}, {1.0}},
      {{0.5}, {2.0}},
  };

  for (const auto& quantization : invalid_cases) {
    EXPECT_FALSE(ComputePressureQuantizer::IsValid(quantization));
  }
}

TEST(ComputePressureQuantizer, IsSame_True) {
  std::vector<std::pair<blink::mojom::ComputePressureQuantization,
                        blink::mojom::ComputePressureQuantization>>
      true_cases = {
          {{{0.1}, {0.2}}, {{0.1}, {0.2}}},
          {{{0.2, 0.5, 0.8}, {0.5}}, {{0.2, 0.5, 0.8}, {0.5}}},
          {{{0.3}, {0.2}}, {{0.1 + 0.1 + 0.1}, {0.1 + 0.1}}},
      };

  for (const auto& quantizations : true_cases) {
    ComputePressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_TRUE(quantizer.IsSame(quantizations.second));
  }
}

TEST(ComputePressureQuantizer, IsSame_False) {
  std::vector<std::pair<blink::mojom::ComputePressureQuantization,
                        blink::mojom::ComputePressureQuantization>>
      false_cases = {
          {{{0.1}, {0.2}}, {{0.2}, {0.2}}},
          {{{0.1}, {0.2}}, {{0.1}, {0.3}}},
          {{{0.1, 0.15}, {0.2}}, {{0.1}, {0.2}}},
          {{{0.1}, {}}, {{0.1}, {0.2}}},
          {{{0.1}, {0.2}}, {{0.1, 0.15}, {0.2}}},
          {{{0.1}, {0.2}}, {{0.1}, {}}},
          {{{0.1}, {0.2}}, {{0.101}, {0.2}}},
          {{{0.1}, {0.2}}, {{0.1}, {0.201}}},
          {{{0.2, 0.5, 0.8}, {0.5}}, {{0.2, 0.6, 0.8}, {0.5}}},
          {{{0.2, 0.5, 0.8}, {0.5}}, {{0.2, 0.5, 0.9}, {0.5}}},
      };

  for (const auto& quantizations : false_cases) {
    ComputePressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_FALSE(quantizer.IsSame(quantizations.second));
  }
}

TEST(ComputePressureQuantizer, Quantize_Empty) {
  ComputePressureQuantizer quantizer;
  auto empty_quantization_ptr =
      blink::mojom::ComputePressureQuantization::New();
  quantizer.Assign(std::move(empty_quantization_ptr));

  EXPECT_EQ(blink::mojom::ComputePressureState(0.5, 0.5),
            quantizer.Quantize({.cpu_utilization = 0.0, .cpu_speed = 0.0}));
  EXPECT_EQ(blink::mojom::ComputePressureState(0.5, 0.5),
            quantizer.Quantize({.cpu_utilization = 1.0, .cpu_speed = 1.0}));
}

TEST(ComputePressureQuantizer, Quantize) {
  ComputePressureQuantizer quantizer;
  auto quantization_ptr = blink::mojom::ComputePressureQuantization::New(
      std::vector<double>{0.2, 0.5, 0.8}, std::vector<double>{0.5});
  quantizer.Assign(std::move(quantization_ptr));

  std::vector<
      std::pair<ComputePressureSample, blink::mojom::ComputePressureState>>
      test_cases = {
          {{0.0, 0.0}, {0.1, 0.25}},    {{1.0, 0.0}, {0.9, 0.25}},
          {{0.0, 1.0}, {0.1, 0.75}},    {{1.0, 1.0}, {0.9, 0.75}},
          {{0.1, 0.1}, {0.1, 0.25}},    {{0.19, 0.19}, {0.1, 0.25}},
          {{0.21, 0.21}, {0.35, 0.25}}, {{0.49, 0.49}, {0.35, 0.25}},
          {{0.51, 0.51}, {0.65, 0.75}}, {{0.79, 0.79}, {0.65, 0.75}},
          {{0.81, 0.81}, {0.9, 0.75}},  {{0.99, 0.99}, {0.9, 0.75}},
      };

  for (const auto& test_case : test_cases) {
    ComputePressureSample input = test_case.first;
    blink::mojom::ComputePressureState expected = test_case.second;
    blink::mojom::ComputePressureState output = quantizer.Quantize(input);

    EXPECT_DOUBLE_EQ(expected.cpu_utilization, output.cpu_utilization) << input;
    EXPECT_DOUBLE_EQ(expected.cpu_speed, output.cpu_speed) << input;
  }
}

}  // namespace content
