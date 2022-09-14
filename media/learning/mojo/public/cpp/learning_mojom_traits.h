// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
#define MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_

#include <vector>

#include "base/component_export.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/common/value.h"
#include "media/learning/mojo/public/mojom/learning_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::LabelledExampleDataView,
                 media::learning::LabelledExample> {
  static const std::vector<media::learning::FeatureValue>& features(
      const media::learning::LabelledExample& e) {
    return e.features;
  }
  static media::learning::TargetValue target_value(
      const media::learning::LabelledExample& e) {
    return e.target_value;
  }

  static bool Read(media::learning::mojom::LabelledExampleDataView data,
                   media::learning::LabelledExample* out_example);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::FeatureValueDataView,
                 media::learning::FeatureValue> {
  static double value(const media::learning::FeatureValue& e) {
    return e.value();
  }
  static bool Read(media::learning::mojom::FeatureValueDataView data,
                   media::learning::FeatureValue* out_feature_value);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::TargetValueDataView,
                 media::learning::TargetValue> {
  static double value(const media::learning::TargetValue& e) {
    return e.value();
  }
  static bool Read(media::learning::mojom::TargetValueDataView data,
                   media::learning::TargetValue* out_target_value);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::ObservationCompletionDataView,
                 media::learning::ObservationCompletion> {
  static media::learning::TargetValue target_value(
      const media::learning::ObservationCompletion& e) {
    return e.target_value;
  }
  static media::learning::WeightType weight(
      const media::learning::ObservationCompletion& e) {
    return e.weight;
  }
  static bool Read(
      media::learning::mojom::ObservationCompletionDataView data,
      media::learning::ObservationCompletion* out_observation_completion);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::TargetHistogramPairDataView,
                 media::learning::TargetHistogramPair> {
  static media::learning::TargetValue target_value(
      const media::learning::TargetHistogramPair& e) {
    return e.target_value;
  }
  static double count(const media::learning::TargetHistogramPair& e) {
    return e.count;
  }
  static bool Read(media::learning::mojom::TargetHistogramPairDataView data,
                   media::learning::TargetHistogramPair* out_pair);
};

template <>
struct COMPONENT_EXPORT(MEDIA_LEARNING_SHARED_TYPEMAP_TRAITS)
    StructTraits<media::learning::mojom::TargetHistogramDataView,
                 media::learning::TargetHistogram> {
  static std::vector<media::learning::TargetHistogramPair> pairs(
      const media::learning::TargetHistogram& e) {
    std::vector<media::learning::TargetHistogramPair> pairs;
    for (auto const& [target_val, count] : e.counts_) {
      pairs.emplace_back(target_val, count);
    }

    return pairs;
  }

  static bool Read(media::learning::mojom::TargetHistogramDataView data,
                   media::learning::TargetHistogram* out_target_histogram);
};

}  // namespace mojo

#endif  // MEDIA_LEARNING_MOJO_PUBLIC_CPP_LEARNING_MOJOM_TRAITS_H_
