// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <random>

#include "absl/random/random.h"

// This program is used in integration tests.

int main() {
  std::seed_seq seed_seq{1234};
  absl::BitGen rng(seed_seq);
  constexpr size_t kSequenceLength = 8;
  for (size_t i = 0; i < kSequenceLength; i++) {
    std::cout << rng() << "\n";
  }
  return 0;
}
