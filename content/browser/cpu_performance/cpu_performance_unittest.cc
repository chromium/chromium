// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cpu_performance/cpu_performance.h"

#include <utility>
#include <vector>

#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using CpuPerformanceTest = testing::Test;
using Tier = cpu_performance::Tier;
using Manufacturer = cpu_performance::Manufacturer;

TEST_F(CpuPerformanceTest, GetTierFromCores) {
  std::vector<std::pair<int, Tier>> tests = {
      {1, Tier::kLow},    {2, Tier::kLow},     {3, Tier::kMid},
      {4, Tier::kMid},    {5, Tier::kHigh},    {8, Tier::kHigh},
      {12, Tier::kHigh},  {13, Tier::kUltra},  {16, Tier::kUltra},
      {96, Tier::kUltra}, {0, Tier::kUnknown}, {-42, Tier::kUnknown},
  };

  for (const auto& [cores, expected_tier] : tests) {
    Tier tier = cpu_performance::GetTierFromCores(cores);
    EXPECT_EQ(expected_tier, tier) << "Failed for " << cores << " core(s)";
  }
}

TEST_F(CpuPerformanceTest, SplitCpuModel) {
  std::vector<std::tuple<std::string_view, Manufacturer, std::string_view>>
      tests = {
          {"Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz", Manufacturer::kIntel,
           "Core i7-10700K"},
          {"Intel® Core™i7-8600HQ CPU @ 3.0GHz", Manufacturer::kIntel,
           "Core i7-8600HQ"},
          {"Intel(R) Processor 5Y70 CPU @ 1.10GHz", Manufacturer::kIntel,
           "5Y70"},
          {"Intel(R) Core(TM) i3 CPU       M 330  @ 2.13GHz",
           Manufacturer::kIntel, "Core i3-330M"},
          {"Intel(R) Core(TM)2 Duo CPU     E4300  @ 1.80GHz",
           Manufacturer::kIntel, "Core 2 Duo E4300"},
          {"Intel(R) Core(TM) i7 CPU       L 620  @ 2.00GHz",
           Manufacturer::kIntel, "Core i7-620LM"},
          {"Intel(R) Core i5 - 10500u (tm) Processor", Manufacturer::kIntel,
           "Core i5-10500u"},
          {"Celeron(R) Dual-Core CPU       T3000  @ 1.80GHz",
           Manufacturer::kIntel, "Celeron T3000"},
          {"AMD Ryzen 7 5800X 8-Core Processor", Manufacturer::kAMD,
           "Ryzen 7 5800X"},
          {"AMD Ryzen 5 3500U with Radeon Vega Mobile Gfx", Manufacturer::kAMD,
           "Ryzen 5 3500U"},
          {
              "AMD A4-9120e RADEON R3, 4 COMPUTE CORES 2C+2G",
              Manufacturer::kAMD,
              "A4-9120e",
          },
          {"AMD A10-4600M APU with Radeon(tm) HD Graphics", Manufacturer::kAMD,
           "A10-4600M"},
          {"AMD FX(tm)-4130 Quad-Core Processor", Manufacturer::kAMD,
           "FX-4130"},
          {"AMD Ryzen 3 3250C 15W with Radeon Graphics", Manufacturer::kAMD,
           "Ryzen 3 3250C"},
          {"AMD Ryzen 5 6600HS Creator Edition", Manufacturer::kAMD,
           "Ryzen 5 6600HS"},
          {"AMD Turion(tm) 64 Mobile Technology MK-36", Manufacturer::kAMD,
           "Turion 64 MK-36"},
          {"AMD® Ryzen™5  3450U Quad Core@", Manufacturer::kAMD,
           "Ryzen 5 3450U"},
          {"Apple M1 ", Manufacturer::kApple, "M1"},
          {"Apple M2 Pro (Virtual)", Manufacturer::kApple, "M2 Pro"},
          // Model clean-up is not yet implemented for these manufacturers.
          {"Microsoft SQ2 @ 3.15 GHz", Manufacturer::kMicrosoft, ""},
          {"Snapdragon(R) X Elite - X1E78100 - Qualcomm(R) Oryon(TM) CPU",
           Manufacturer::kQualcomm, ""},
          {"Snapdragon (TM) 7c @ 2.40 GHz", Manufacturer::kQualcomm, ""},
          {"MediaTek Dimensity 9200", Manufacturer::kMediaTek, ""},
          {"Samsung Exynos 2100", Manufacturer::kSamsung, ""},
          {"Unknown CPU", Manufacturer::kUnknown, ""},
      };

  for (const auto& [cpu_model, expected_manufacturer, expected_model] : tests) {
    auto [manufacturer, model] = cpu_performance::SplitCpuModel(cpu_model);
    EXPECT_EQ(expected_manufacturer, manufacturer)
        << "Failed for '" << cpu_model << "'";
    EXPECT_EQ(expected_model, model) << "Failed for '" << cpu_model << "'";
  }
}

TEST_F(CpuPerformanceTest, GetTierFromCpuInfo) {
  std::vector<std::tuple<int, std::string_view, Tier>> tests = {
      // Special cases, unknown.
      {-42, "Some imaginary processor", Tier::kUnknown},
      {0, "Some unknown processor", Tier::kUnknown},
      // Single core.
      {1, "Intel(R) Celeron(R) CPU          450  @ 2.20GHz", Tier::kLow},
      {1, "AMD Athlon(tm) Processor 1640B", Tier::kLow},
      // AMD.
      {2, "AMD E2-9000e RADEON R2, 4 COMPUTE CORES 2C+2G", Tier::kLow},
      {2, "AMD A4-9120C RADEON R4, 5 COMPUTE CORES 2C+3G", Tier::kLow},
      {2, "AMD Athlon(tm) 64 X2 Dual Core Processor 5000+", Tier::kLow},
      {4, "AMD Ryzen 3 3200U with Radeon Vega Mobile Gfx", Tier::kMid},
      {4, "AMD Ryzen 3 3100 4-Core Processor", Tier::kHigh},
      // Intel.
      {2, "Intel(R) Atom(TM) CPU Z520   @ 1.33GHz", Tier::kLow},
      {2, "Intel(R) Celeron(R) CPU J3355 @ 2.00GHz", Tier::kLow},
      {2, "Intel(R) Core(TM)2 Duo CPU     P8600  @ 2.40GHz", Tier::kLow},
      {2, "Intel(R) Pentium(R) CPU        P6100  @ 2.00GHz", Tier::kLow},
      {2, "Intel(R) Celeron(R) CPU B830 @ 1.80GHz", Tier::kLow},
      {2, "Intel(R) Celeron(R) N4000 CPU @ 1.10GHz", Tier::kMid},
      {4, "Intel(R) Celeron(R) CPU  N3160  @ 1.60GHz", Tier::kLow},
      {4, "Intel(R) N100", Tier::kHigh},
      {4, "Intel(R) Atom(TM) x7425E", Tier::kHigh},
      {8, "11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz", Tier::kHigh},
      {8, "Intel(R) Core(TM) Ultra 5 226V", Tier::kUltra},
      {16, "Intel(R) Core(TM) Ultra 7 155H", Tier::kUltra},
      // Apple.
      {6, "Apple A18 Pro", Tier::kHigh},
      {8, "Apple M1", Tier::kUltra},
      // Qualcomm.
      {10, "Snapdragon(R) X Plus - X1P64100 - Qualcomm(R) Oryon(TM) CPU",
       Tier::kHigh},
      {12, "Snapdragon(R) X Elite - X1E78100 - Qualcomm(R) Oryon(TM) CPU",
       Tier::kUltra},
      {8, "MediaTek Dimensity 9200", Tier::kHigh},
      {8, "Samsung Exynos 2100", Tier::kHigh},
      {2, "Unknown CPU", Tier::kLow},
      {4, "Unknown CPU", Tier::kMid},
      {8, "Unknown CPU", Tier::kHigh},
      {16, "Unknown CPU", Tier::kUltra},
  };

  for (const auto& [cores, model, expected_tier] : tests) {
    Tier tier = cpu_performance::GetTierFromCpuInfo(model, cores);
    EXPECT_EQ(expected_tier, tier)
        << "Failed for '" << model << "' with " << cores << " core(s)";
  }
}

TEST_F(CpuPerformanceTest, Initialize) {
  int cores = base::SysInfo::NumberOfProcessors();
  std::string cpu_model = base::SysInfo::CPUModelName();

  cpu_performance::Tier tier_simple = cpu_performance::GetTierFromCores(cores);
  cpu_performance::Tier tier_accurate =
      cpu_performance::GetTierFromCpuInfo(cpu_model, cores);

  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kCpuPerformance);

  // Before initialization has started, it should return Tier::kUnknown.
  EXPECT_EQ(cpu_performance::Tier::kUnknown, cpu_performance::GetTier());

  // Before initialization is complete, it should use at least the simple
  // implementation. It may use the accurate one, if initialization wins the
  // data race here.
  cpu_performance::Initialize();
  EXPECT_THAT(cpu_performance::GetTier(),
              testing::AnyOf(tier_simple, tier_accurate));

  // After initialization is complete (i.e., when the asynchronous task has been
  // executed), it should use the accurate implementation.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(tier_accurate, cpu_performance::GetTier());
}

}  // namespace content
