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
}  // namespace mojo
