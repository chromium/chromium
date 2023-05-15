// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/tuneable.h"

#include <algorithm>
#include <random>

#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"

namespace {

// Get the finch parameter `name`, and clamp it to the given values.  Return
// `default_value` if there is no parameter, or if the experiment is off.
template <typename T>
T GetParam(const char* name,
           T minimum_value,
           T default_value,
           T maximum_value) {
  return static_cast<T>(GetParam<int>(name, static_cast<int>(minimum_value),
                                      static_cast<int>(default_value),
                                      static_cast<int>(maximum_value)));
}

template <>
int GetParam<int>(const char* name,
                  int minimum_value,
                  int default_value,
                  int maximum_value) {
  return std::clamp(
      base::FeatureParam<int>(&::media::kMediaOptimizer, name, default_value)
          .Get(),
      minimum_value, maximum_value);
}

template <>
base::TimeDelta GetParam<base::TimeDelta>(const char* name,
                                          base::TimeDelta minimum_value,
                                          base::TimeDelta default_value,
                                          base::TimeDelta maximum_value) {
  return base::Milliseconds(GetParam<int>(name, minimum_value.InMilliseconds(),
                                          default_value.InMilliseconds(),
                                          maximum_value.InMilliseconds()));
}

}  // namespace

namespace media {

template <typename T>
Tuneable<T>::Tuneable(const char* name,
                      T minimum_value,
                      T default_value,
                      T maximum_value) {
  // Fetch the finch-provided value, clamped to the min, max and defaulted to
  // the hardcoded default if it's unset.
  t_ = GetParam<T>(name, minimum_value, default_value, maximum_value);
}

template <typename T>
Tuneable<T>::~Tuneable() = default;

// All allowed Tuneable types.  Be sure that GenerateRandom() and GetParam()
// do something sane for any type you add.
template class MEDIA_EXPORT Tuneable<int>;
template class MEDIA_EXPORT Tuneable<base::TimeDelta>;
template class MEDIA_EXPORT Tuneable<size_t>;

}  // namespace media
