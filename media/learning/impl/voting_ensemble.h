// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_VOTING_ENSEMBLE_H_
#define MEDIA_LEARNING_IMPL_VOTING_ENSEMBLE_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "media/learning/impl/model.h"

namespace media {
namespace learning {

// Ensemble classifier.  Takes multiple models and returns an aggregate of the
// individual predictions.
class COMPONENT_EXPORT(LEARNING_IMPL) VotingEnsemble : public Model {
 public:
  VotingEnsemble(std::vector<std::unique_ptr<Model>> models);
  ~VotingEnsemble() override;

  // Model
  TargetHistogram PredictDistribution(const FeatureVector& instance) override;

 private:
  std::vector<std::unique_ptr<Model>> models_;

  DISALLOW_COPY_AND_ASSIGN(VotingEnsemble);
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_VOTING_ENSEMBLE_H_
