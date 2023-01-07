// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <vector>

#include "benchmark/benchmark.h"
#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"
#include "absl/random/random.h"

namespace absl {
namespace {

template <typename T>
static void BM_bitwidth(benchmark::State& state) {
  const int count = state.range(0);

  absl::BitGen rng;
  std::vector<T> values;
  values.reserve(count);
  for (int i = 0; i < count; ++i) {
    values.push_back(absl::Uniform<T>(rng, 0, std::numeric_limits<T>::max()));
  }

  while (state.KeepRunningBatch(count)) {
    for (int i = 0; i < count; ++i) {
      benchmark::DoNotOptimize(values[i]);
    }
  }
}
BENCHMARK_TEMPLATE(BM_bitwidth, uint8_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth, uint16_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth, uint32_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth, uint64_t)->Range(1, 1 << 20);

template <typename T>
static void BM_bitwidth_nonzero(benchmark::State& state) {
  const int count = state.range(0);

  absl::BitGen rng;
  std::vector<T> values;
  values.reserve(count);
  for (int i = 0; i < count; ++i) {
    values.push_back(absl::Uniform<T>(rng, 1, std::numeric_limits<T>::max()));
  }

  while (state.KeepRunningBatch(count)) {
    for (int i = 0; i < count; ++i) {
      const T value = values[i];
      ABSL_ASSUME(value > 0);
      benchmark::DoNotOptimize(value);
    }
  }
}
BENCHMARK_TEMPLATE(BM_bitwidth_nonzero, uint8_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth_nonzero, uint16_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth_nonzero, uint32_t)->Range(1, 1 << 20);
BENCHMARK_TEMPLATE(BM_bitwidth_nonzero, uint64_t)->Range(1, 1 << 20);

}  // namespace
}  // namespace absl
