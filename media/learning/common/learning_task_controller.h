// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_
#define MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/common/target_histogram.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace media {
namespace learning {

// Wrapper struct for completing an observation via LearningTaskController.
// Most callers will just send in a TargetValue, so this lets us provide a
// default weight.  Further, a few callers will add optional data, like the UKM
// SourceId, which most callers don't care about.
struct ObservationCompletion {
  ObservationCompletion() = default;
  /* implicit */ ObservationCompletion(const TargetValue& target,
                                       WeightType w = 1.)
      : target_value(target), weight(w) {}

  TargetValue target_value;
  WeightType weight;

  // Mostly for gmock matchers.
  bool operator==(const ObservationCompletion& rhs) const {
    return target_value == rhs.target_value && weight == rhs.weight;
  }
};

// Client for a single learning task.  Intended to be the primary API for client
// code that generates FeatureVectors / requests predictions for a single task.
// The API supports sending in an observed FeatureVector without a target value,
// so that framework-provided features (FeatureProvider) can be snapshotted at
// the right time.  One doesn't generally want to wait until the TargetValue is
// observed to do that.
class COMPONENT_EXPORT(LEARNING_COMMON) LearningTaskController {
 public:
  using PredictionCB =
      base::OnceCallback<void(const std::optional<TargetHistogram>& predicted)>;

  LearningTaskController() = default;

  LearningTaskController(const LearningTaskController&) = delete;
  LearningTaskController& operator=(const LearningTaskController&) = delete;

  virtual ~LearningTaskController() = default;

  // Start a new observation.  Call this at the time one would try to predict
  // the TargetValue.  This lets the framework snapshot any framework-provided
  // feature values at prediction time.  Later, if you want to turn these
  // features into an example for training a model, then call
  // CompleteObservation with the same id and an ObservationCompletion.
  // Otherwise, call CancelObservation with |id|.  It's also okay to destroy the
  // controller with outstanding observations; these will be cancelled if no
  // |default_target| was specified, or completed with |default_target|.
  //
  // TODO(liberato): This should optionally take a callback to receive a
  // prediction for the FeatureVector.
  // TODO(liberato): See if this ends up generating smaller code with pass-by-
  // value or with |FeatureVector&&|, once we have callers that can actually
  // benefit from it.
  virtual void BeginObservation(
      base::UnguessableToken id,
      const FeatureVector& features,
      const std::optional<TargetValue>& default_target = std::nullopt,
      const std::optional<ukm::SourceId>& source_id = std::nullopt) = 0;

  // Complete an observation by sending a completion.
  virtual void CompleteObservation(base::UnguessableToken id,
                                   const ObservationCompletion& completion) = 0;

  // Notify the LearningTaskController that no completion will be sent.
  virtual void CancelObservation(base::UnguessableToken id) = 0;

  // Update the default target value for |id|.  This can change a previously
  // specified default value to something else, add one where one wasn't
  // specified before, or un-set it.  In the last case, the observation will be
  // cancelled rather than completed if |this| is destroyed, just as if no
  // default value was given.
  virtual void UpdateDefaultTarget(
      base::UnguessableToken id,
      const std::optional<TargetValue>& default_target) = 0;

  // Returns the LearningTask associated with |this|.
  virtual const LearningTask& GetLearningTask() = 0;

  // Asynchronously predicts distribution for given |features|. |callback| will
  // receive a std::nullopt prediction when model is not available. |callback|
  // may be called immediately without posting.
  virtual void PredictDistribution(const FeatureVector& features,
                                   PredictionCB callback) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_LEARNING_TASK_CONTROLLER_H_
