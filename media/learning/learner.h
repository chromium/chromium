// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_LEARNER_H_
#define MEDIA_LEARNING_LEARNER_H_

#include "base/values.h"
#include "media/base/media_export.h"
#include "media/learning/instance.h"

namespace media {
namespace learning {

// Base class for a thing that takes examples of the form {features, target},
// and trains a model to predict the target given the features.  The target may
// be either nominal (classification) or numeric (regression), though this must
// be chosen in advance when creating the learner via LearnerFactory.
class MEDIA_EXPORT Learner {
 public:
  virtual ~Learner() = default;

  // Tell the learner that |instance| has been observed with the target value
  // |target| during training.
  virtual void AddExample(const Instance& instance,
                          const TargetValue& target) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_LEARNER_H_
