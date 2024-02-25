// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_INTERNAL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_INTERNAL_H_

#include <type_traits>
#include <utility>

#include "third_party/abseil-cpp/absl/types/variant.h"

// Adapted from
// components/performance_manager/public/resource_attribution/type_helpers.h
namespace content::internal {

// The constant IsVariantAlternative<T, V>::value is true iff T is one of the
// alternative types of variant V.
template <typename T, typename V, size_t I = absl::variant_size<V>::value>
struct IsVariantAlternative
    : std::disjunction<
          std::is_same<T, typename absl::variant_alternative<I - 1, V>::type>,
          IsVariantAlternative<T, V, I - 1>> {};

template <typename T, typename V>
struct IsVariantAlternative<T, V, 0> : std::false_type {};

// For SFINAE, a template using EnableIfIsVariantAlternative<T, V> will only
// match if T is one of the alternative types of variant V.
template <typename T, typename V>
using EnableIfIsVariantAlternative =
    std::enable_if_t<IsVariantAlternative<T, V>::value, bool>;

}  // namespace content::internal

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_STORE_SOURCE_RESULT_INTERNAL_H_
