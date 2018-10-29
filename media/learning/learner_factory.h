// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_LEARNER_FACTORY_H_
#define MEDIA_LEARNING_LEARNER_FACTORY_H_

#include <string>

#include "media/base/media_export.h"
#include "media/learning/learing_task.h"
#include "media/learning/learner.h"

namespace media {
namespace learning {

// Factory class for learner instances.
class MEDIA_EXPORT LearnerFactory {
 public:
  virtual ~LearnerFactory() = default;

  // Provide a learner that matches |task|.
  virtual std::unique_ptr<Learner> CreateLearner(const LearningTask& task) = 0;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_LEARNER_FACTORY_H_
