// Copyright 2019 The Abseil Authors.
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

#ifndef ABSL_RANDOM_INTERNAL_DISTRIBUTIONS_H_
#define ABSL_RANDOM_INTERNAL_DISTRIBUTIONS_H_

#include <type_traits>

#include "absl/meta/type_traits.h"
#include "absl/random/internal/distribution_caller.h"
#include "absl/random/internal/traits.h"
#include "absl/random/internal/uniform_helper.h"

namespace absl {
namespace random_internal {
template <typename D>
struct DistributionFormatTraits;

// UniformImpl implements the core logic of the Uniform<T> call, which is to
// select the correct distribution type, compute the bounds based on the
// interval tag, and then generate a value.
template <typename NumType, typename TagType, typename URBG>
NumType UniformImpl(TagType tag,
                    URBG& urbg,  // NOLINT(runtime/references)
                    NumType lo, NumType hi) {
  static_assert(
      std::is_arithmetic<NumType>::value,
      "absl::Uniform<T>() must use an integer or real parameter type.");

  using distribution_t =
      typename std::conditional<std::is_integral<NumType>::value,
                                absl::uniform_int_distribution<NumType>,
                                absl::uniform_real_distribution<NumType>>::type;
  using format_t = random_internal::DistributionFormatTraits<distribution_t>;

  auto a = random_internal::uniform_lower_bound<NumType>(tag, lo, hi);
  auto b = random_internal::uniform_upper_bound<NumType>(tag, lo, hi);
  // TODO(lar): it doesn't make a lot of sense to ask for a random number in an
  // empty range.  Right now we just return a boundary--even though that
  // boundary is not an acceptable value!  Is there something better we can do
  // here?

  using gen_t = absl::decay_t<URBG>;
  if (a > b) return a;
  return DistributionCaller<gen_t>::template Call<distribution_t, format_t>(
      &urbg, a, b);
}

// In the absence of an explicitly provided return-type, the template
// "uniform_inferred_return_t<A, B>" is used to derive a suitable type, based on
// the data-types of the endpoint-arguments {A lo, B hi}.
//
// Given endpoints {A lo, B hi}, one of {A, B} will be chosen as the
// return-type, if one type can be implicitly converted into the other, in a
// lossless way. The template "is_widening_convertible" implements the
// compile-time logic for deciding if such a conversion is possible.
//
// If no such conversion between {A, B} exists, then the overload for
// absl::Uniform() will be discarded, and the call will be ill-formed.
// Return-type for absl::Uniform() when the return-type is inferred.
template <typename A, typename B>
using uniform_inferred_return_t =
    absl::enable_if_t<absl::disjunction<is_widening_convertible<A, B>,
                                        is_widening_convertible<B, A>>::value,
                      typename std::conditional<
                          is_widening_convertible<A, B>::value, B, A>::type>;

}  // namespace random_internal
}  // namespace absl

#endif  // ABSL_RANDOM_INTERNAL_DISTRIBUTIONS_H_
