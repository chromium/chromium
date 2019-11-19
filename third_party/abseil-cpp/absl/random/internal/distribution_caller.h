//
// Copyright 2018 The Abseil Authors.
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
//

#ifndef ABSL_RANDOM_INTERNAL_DISTRIBUTION_CALLER_H_
#define ABSL_RANDOM_INTERNAL_DISTRIBUTION_CALLER_H_

#include <utility>

namespace absl {
namespace random_internal {

// DistributionCaller provides an opportunity to overload the general
// mechanism for calling a distribution, allowing for mock-RNG classes
// to intercept such calls.
template <typename URBG>
struct DistributionCaller {
  // Call the provided distribution type. The parameters are expected
  // to be explicitly specified.
  // DistrT is the distribution type.
  // FormatT is the formatter type:
  //
  // struct FormatT {
  //   using result_type = distribution_t::result_type;
  //   static std::string FormatCall(
  //       const distribution_t& distr,
  //       absl::Span<const result_type>);
  //
  //   static std::string FormatExpectation(
  //       absl::string_view match_args,
  //       absl::Span<const result_t> results);
  // }
  //
  template <typename DistrT, typename FormatT, typename... Args>
  static typename DistrT::result_type Call(URBG* urbg, Args&&... args) {
    DistrT dist(std::forward<Args>(args)...);
    return dist(*urbg);
  }
};

}  // namespace random_internal
}  // namespace absl

#endif  // ABSL_RANDOM_INTERNAL_DISTRIBUTION_CALLER_H_
