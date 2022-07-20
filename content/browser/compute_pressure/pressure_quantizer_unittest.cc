// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_quantizer.h"

#include <utility>
#include <vector>

#include "services/device/public/mojom/pressure_state.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"

namespace content {

using blink::mojom::PressureQuantization;
using device::mojom::PressureState;

TEST(PressureQuantizer, IsValid_Valid) {
  std::vector<PressureQuantization> valid_cases = {
      PressureQuantization{},
      PressureQuantization{{0.5}},
      PressureQuantization{{0.2, 0.5, 0.8}},
  };

  for (const auto& quantization : valid_cases) {
    EXPECT_TRUE(PressureQuantizer::IsValid(quantization));
  }
}

TEST(PressureQuantizer, IsValid_Invalid) {
  std::vector<PressureQuantization> invalid_cases = {
      PressureQuantization{
          {0.2, 0.3, 0.4, 0.5}},  // Too many utilization thresholds.
      PressureQuantization{{0.2, 0.8, 0.5}},  // Incorrectly sorted thresholds.
      PressureQuantization{{-1.0}},           // Threshold outside range.
      PressureQuantization{{0.0}},
      PressureQuantization{{1.0}},
      PressureQuantization{{2.0}},
  };

  for (const auto& quantization : invalid_cases) {
    EXPECT_FALSE(PressureQuantizer::IsValid(quantization));
  }
}

TEST(PressureQuantizer, IsSame_True) {
  std::vector<std::pair<PressureQuantization, PressureQuantization>>
      true_cases = {
          {PressureQuantization{{0.1}}, PressureQuantization{{0.1}}},
          {PressureQuantization{{0.2, 0.5, 0.8}},
           PressureQuantization{{0.2, 0.5, 0.8}}},
          {PressureQuantization{{0.3}},
           PressureQuantization{{0.1 + 0.1 + 0.1}}},
      };

  for (const auto& quantizations : true_cases) {
    PressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_TRUE(quantizer.IsSame(quantizations.second));
  }
}

TEST(PressureQuantizer, IsSame_False) {
  std::vector<std::pair<PressureQuantization, PressureQuantization>>
      false_cases = {
          {PressureQuantization{{0.1}}, PressureQuantization{{0.2}}},
          {PressureQuantization{{0.1, 0.15}}, PressureQuantization{{0.1}}},
          {PressureQuantization{{0.1}}, PressureQuantization{{0.1, 0.15}}},
          {PressureQuantization{{0.1}}, PressureQuantization{{0.101}}},
          {PressureQuantization{{0.2, 0.5, 0.8}},
           PressureQuantization{{0.2, 0.6, 0.8}}},
          {PressureQuantization{{0.2, 0.5, 0.8}},
           PressureQuantization{{0.2, 0.5, 0.9}}},
      };

  for (const auto& quantizations : false_cases) {
    PressureQuantizer quantizer;
    quantizer.Assign(quantizations.first.Clone());
    EXPECT_FALSE(quantizer.IsSame(quantizations.second));
  }
}

TEST(PressureQuantizer, Quantize_Empty) {
  PressureQuantizer quantizer;
  auto empty_quantization_ptr = PressureQuantization::New();
  quantizer.Assign(std::move(empty_quantization_ptr));

  EXPECT_EQ(PressureState{0.5}, quantizer.Quantize(PressureState{0.0}.Clone()));
  EXPECT_EQ(PressureState{0.5}, quantizer.Quantize(PressureState{1.0}.Clone()));
}

TEST(PressureQuantizer, Quantize) {
  PressureQuantizer quantizer;
  auto quantization_ptr =
      PressureQuantization::New(std::vector<double>{0.2, 0.5, 0.8});
  quantizer.Assign(std::move(quantization_ptr));

  std::vector<std::pair<PressureState, PressureState>> test_cases = {
      {PressureState{0.0}, PressureState{0.1}},
      {PressureState{1.0}, PressureState{0.9}},
      {PressureState{0.1}, PressureState{0.1}},
      {PressureState{0.19}, PressureState{0.1}},
      {PressureState{0.21}, PressureState{0.35}},
      {PressureState{0.49}, PressureState{0.35}},
      {PressureState{0.51}, PressureState{0.65}},
      {PressureState{0.79}, PressureState{0.65}},
      {PressureState{0.81}, PressureState{0.9}},
      {PressureState{0.99}, PressureState{0.9}},
  };

  for (const auto& test_case : test_cases) {
    PressureState input = test_case.first;
    PressureState expected = test_case.second;
    PressureState output = quantizer.Quantize(input.Clone());

    EXPECT_DOUBLE_EQ(expected.cpu_utilization, output.cpu_utilization)
        << "Input cpu_utilization is: " << input.cpu_utilization;
  }
}

}  // namespace content
