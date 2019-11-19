// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_
#define MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_

#include <set>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/model.h"
#include "media/learning/impl/target_histogram.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace media {
namespace learning {

// Helper class to report on predicted distrubutions vs target distributions.
// Use DistributionReporter::Create() to create one that's appropriate for a
// specific learning task.
class COMPONENT_EXPORT(LEARNING_IMPL) DistributionReporter {
 public:
  // Extra information provided to the reporter for each prediction.
  struct PredictionInfo {
    // What value was observed?
    TargetValue observed;

    // UKM source id to use when logging this result.
    // This will be filled in by the LearningTaskController.  For example, the
    // MojoLearningTaskControllerService will be created in the browser by the
    // MediaMetricsProvider, which gets the SourceId via callback from the
    // RenderFrameHostDelegate on construction.
    //
    // TODO(liberato): Right now, this is not filled in anywhere.  When the
    // mojo service is created (MediaMetricsProvider), record the source id and
    // memorize it in any MojoLearningTaskControllerService that's created by
    // the MediaMetricsProvider, either directly or in a wrapper for the
    // mojo controller.
    ukm::SourceId source_id = ukm::kInvalidSourceId;

    // Total weight of the training data used to create this model.
    double total_training_weight = 0.;

    // Total number of examples (unweighted) in the training set.
    size_t total_training_examples = 0u;

    // TODO(liberato): Move the feature subset here.
  };

  // Create a DistributionReporter that's suitable for |task|.
  static std::unique_ptr<DistributionReporter> Create(const LearningTask& task);

  virtual ~DistributionReporter();

  // Returns a prediction CB that will be compared to |prediction_info.observed|
  // TODO(liberato): This is too complicated.  Skip the callback and just call
  // us with the predicted value.
  virtual Model::PredictionCB GetPredictionCallback(
      const PredictionInfo& prediction_info);

  // Set the subset of features that is being used to train the model.  This is
  // used for feature importance measuremnts.
  //
  // For example, sending in the set [0, 3, 7] would indicate that the model was
  // trained with task().feature_descriptions[0, 3, 7] only.
  //
  // Note that UMA reporting only supports single feature subsets.
  void SetFeatureSubset(const std::set<int>& feature_indices);

 protected:
  DistributionReporter(const LearningTask& task);

  const LearningTask& task() const { return task_; }

  // Implemented by subclasses to report a prediction.
  virtual void OnPrediction(const PredictionInfo& prediction_info,
                            TargetHistogram predicted) = 0;

  const base::Optional<std::set<int>>& feature_indices() const {
    return feature_indices_;
  }

 private:
  LearningTask task_;

  // If provided, then these are the features that are used to train the model.
  // Otherwise, we assume that all features are used.
  base::Optional<std::set<int>> feature_indices_;

  base::WeakPtrFactory<DistributionReporter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DistributionReporter);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_DISTRIBUTION_REPORTER_H_
