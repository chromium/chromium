// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/mojo/public/cpp/learning_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<media::learning::mojom::LabelledExampleDataView,
                  media::learning::LabelledExample>::
    Read(media::learning::mojom::LabelledExampleDataView data,
         media::learning::LabelledExample* out_example) {
  out_example->features.clear();
  if (!data.ReadFeatures(&out_example->features))
    return false;

  if (!data.ReadTargetValue(&out_example->target_value))
    return false;

  return true;
}

// static
bool StructTraits<media::learning::mojom::FeatureValueDataView,
                  media::learning::FeatureValue>::
    Read(media::learning::mojom::FeatureValueDataView data,
         media::learning::FeatureValue* out_feature_value) {
  *out_feature_value = media::learning::FeatureValue(data.value());
  return true;
}

// static
bool StructTraits<media::learning::mojom::TargetValueDataView,
                  media::learning::TargetValue>::
    Read(media::learning::mojom::TargetValueDataView data,
         media::learning::TargetValue* out_target_value) {
  *out_target_value = media::learning::TargetValue(data.value());
  return true;
}

// static
bool StructTraits<media::learning::mojom::ObservationCompletionDataView,
                  media::learning::ObservationCompletion>::
    Read(media::learning::mojom::ObservationCompletionDataView data,
         media::learning::ObservationCompletion* out_observation_completion) {
  if (!data.ReadTargetValue(&out_observation_completion->target_value))
    return false;
  out_observation_completion->weight = data.weight();
  return true;
}

// static
bool StructTraits<media::learning::mojom::TargetHistogramPairDataView,
                  media::learning::TargetHistogramPair>::
    Read(media::learning::mojom::TargetHistogramPairDataView data,
         media::learning::TargetHistogramPair* out_pair) {
  if (!data.ReadTargetValue(&out_pair->target_value))
    return false;
  out_pair->count = data.count();
  return true;
}

// static
bool StructTraits<media::learning::mojom::TargetHistogramDataView,
                  media::learning::TargetHistogram>::
    Read(media::learning::mojom::TargetHistogramDataView data,
         media::learning::TargetHistogram* out_target_histogram) {
  ArrayDataView<media::learning::mojom::TargetHistogramPairDataView> pairs;
  data.GetPairsDataView(&pairs);
  if (pairs.is_null())
    return false;

  for (size_t i = 0; i < pairs.size(); ++i) {
    media::learning::mojom::TargetHistogramPairDataView pair_data;
    pairs.GetDataView(i, &pair_data);
    media::learning::TargetValue value;
    if (!pair_data.ReadTargetValue(&value))
      return false;

    out_target_histogram->counts_.emplace(std::move(value), pair_data.count());
  }

  return true;
}

}  // namespace mojo
