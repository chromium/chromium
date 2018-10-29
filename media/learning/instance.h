// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_INSTANCE_H_
#define MEDIA_LEARNING_INSTANCE_H_

#include <ostream>
#include <vector>

#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/learning/value.h"

namespace media {
namespace learning {

// One instance == group of feature values.
struct MEDIA_EXPORT Instance {
  Instance();
  ~Instance();
  // Declare a no-exception move constructor so that std::vector will use it.
  Instance(Instance&& rhs) noexcept;

  // It's up to you to add the right number of features to match the learner
  // description.  Otherwise, the learner will ignore (training) or lie to you
  // (inference), silently.
  std::vector<FeatureValue> features;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& out,
                                      const Instance& instance);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_INSTANCE_H_
