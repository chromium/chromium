// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_quantizer.h"

#include <utility>
#include <vector>

#include "services/device/public/mojom/compute_pressure_state.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

using blink::mojom::ComputePressureQuantization;
using device::mojom::ComputePressureState;

TEST(ComputePressureQuantizer, IsValid_Valid) {
  std::vector<ComputePressureQuantization> valid_cases = {
      ComputePressureQuantization{},
      ComputePressureQuantization{{0.5}},
      ComputePressureQuantization{{0.2, 0.5, 0.8}},
  };

  for (const auto& quantization : valid_cases) {
    EXPECT_TRUE(ComputePressureQuantizer::IsValid(quantization));
  }
}

TEST(ComputePressureQuantizer, IsValid_Invalid) {
  std::vector<ComputePressureQuantization> invalid_cases = {
      ComputePressureQuantization{
          {0.2, 0.3, 0.4, 0.5}},  // Too many utilization thresholds.
      ComputePressureQuantization{
          {0.2, 0.8, 0.5}},                 // Incorrectly sorted thresholds.
      ComputePressureQuantization{{-1.0}},  // Threshold outside range.
      ComputePressureQuantization{{0.0}},
      ComputePressureQuantization{{1.0}},
      ComputePressureQuantization{{2.0}},
  };

  for (const auto& quantization : invalid_cases) {
    EXPECT_FALSE(ComputePressureQuantizer::IsValid(quantization));
  }
}

TEST(ComputePressureQuantizer, IsSame_True) {
  std::vector<
      std::pair<ComputePressureQuantization, ComputePressureQuantization>>
      true_cases = {
          {ComputePressureQuantization{{0.1}},
           ComputePressureQuantization{{0.1}}},
          {ComputePressureQuantization{{0.2, 0.5, 0.8}},
           ComputePressureQuantization{{0.2, 0.5, 0.8}}},
          {ComputePressureQuantization{{0.3}},
           ComputePressureQuantization{{0.1 + 0.1 + 0.1}}},
      };

  for (const auto& quantizations : true_cases) {
    ComputePressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_TRUE(quantizer.IsSame(quantizations.second));
  }
}

TEST(ComputePressureQuantizer, IsSame_False) {
  std::vector<
      std::pair<ComputePressureQuantization, ComputePressureQuantization>>
      false_cases = {
          {ComputePressureQuantization{{0.1}},
           ComputePressureQuantization{{0.2}}},
          {ComputePressureQuantization{{0.1, 0.15}},
           ComputePressureQuantization{{0.1}}},
          {ComputePressureQuantization{{0.1}},
           ComputePressureQuantization{{0.1, 0.15}}},
          {ComputePressureQuantization{{0.1}},
           ComputePressureQuantization{{0.101}}},
          {ComputePressureQuantization{{0.2, 0.5, 0.8}},
           ComputePressureQuantization{{0.2, 0.6, 0.8}}},
          {ComputePressureQuantization{{0.2, 0.5, 0.8}},
           ComputePressureQuantization{{0.2, 0.5, 0.9}}},
      };

  for (const auto& quantizations : false_cases) {
    ComputePressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_FALSE(quantizer.IsSame(quantizations.second));
  }
}

TEST(ComputePressureQuantizer, Quantize_Empty) {
  ComputePressureQuantizer quantizer;
  auto empty_quantization_ptr = ComputePressureQuantization::New();
  quantizer.Assign(std::move(empty_quantization_ptr));

  EXPECT_EQ(ComputePressureState{0.5},
            quantizer.Quantize(ComputePressureState{0.0}.Clone()));
  EXPECT_EQ(ComputePressureState{0.5},
            quantizer.Quantize(ComputePressureState{1.0}.Clone()));
}

TEST(ComputePressureQuantizer, Quantize) {
  ComputePressureQuantizer quantizer;
  auto quantization_ptr =
      ComputePressureQuantization::New(std::vector<double>{0.2, 0.5, 0.8});
  quantizer.Assign(std::move(quantization_ptr));

  std::vector<std::pair<ComputePressureState, ComputePressureState>>
      test_cases = {
          {ComputePressureState{0.0}, ComputePressureState{0.1}},
          {ComputePressureState{1.0}, ComputePressureState{0.9}},
          {ComputePressureState{0.1}, ComputePressureState{0.1}},
          {ComputePressureState{0.19}, ComputePressureState{0.1}},
          {ComputePressureState{0.21}, ComputePressureState{0.35}},
          {ComputePressureState{0.49}, ComputePressureState{0.35}},
          {ComputePressureState{0.51}, ComputePressureState{0.65}},
          {ComputePressureState{0.79}, ComputePressureState{0.65}},
          {ComputePressureState{0.81}, ComputePressureState{0.9}},
          {ComputePressureState{0.99}, ComputePressureState{0.9}},
      };

  for (const auto& test_case : test_cases) {
    ComputePressureState input = test_case.first;
    ComputePressureState expected = test_case.second;
    ComputePressureState output = quantizer.Quantize(input.Clone());

    EXPECT_DOUBLE_EQ(expected.cpu_utilization, output.cpu_utilization)
        << "Input cpu_utilization is: " << input.cpu_utilization;
  }
}

}  // namespace content
