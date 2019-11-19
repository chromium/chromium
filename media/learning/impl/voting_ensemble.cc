// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/voting_ensemble.h"

namespace media {
namespace learning {

VotingEnsemble::VotingEnsemble(std::vector<std::unique_ptr<Model>> models)
    : models_(std::move(models)) {}

VotingEnsemble::~VotingEnsemble() = default;

TargetHistogram VotingEnsemble::PredictDistribution(
    const FeatureVector& instance) {
  TargetHistogram distribution;

  for (auto iter = models_.begin(); iter != models_.end(); iter++)
    distribution += (*iter)->PredictDistribution(instance);

  return distribution;
}

}  // namespace learning
}  // namespace media
