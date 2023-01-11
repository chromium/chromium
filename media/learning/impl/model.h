// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_MODEL_H_
#define MEDIA_LEARNING_IMPL_MODEL_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/target_histogram.h"

namespace media {
namespace learning {

// One trained model, useful for making predictions.
// TODO(liberato): Provide an API for incremental update, for those models that
// can support it.
class COMPONENT_EXPORT(LEARNING_IMPL) Model {
 public:
  // Callback for asynchronous predictions.
  using PredictionCB = base::OnceCallback<void(TargetHistogram predicted)>;

  virtual ~Model() = default;

  virtual TargetHistogram PredictDistribution(
      const FeatureVector& instance) = 0;

  // TODO(liberato): Consider adding an async prediction helper.
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_MODEL_H_
