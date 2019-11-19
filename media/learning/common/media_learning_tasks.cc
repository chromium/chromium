// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/media_learning_tasks.h"

namespace media {
namespace learning {

static const LearningTask& GetWillPlayTask() {
  static LearningTask task_;
  if (!task_.feature_descriptions.size()) {
    task_.name = "MediaLearningWillPlay";
    // TODO(liberato): fill in the rest here, once we have the features picked.
  }

  return task_;
}

// static
const LearningTask& MediaLearningTasks::Get(Id id) {
  switch (id) {
    case Id::kWillPlay:
      return GetWillPlayTask();
  }
}

// static
void MediaLearningTasks::Register(
    base::RepeatingCallback<void(const LearningTask&)> cb) {
  cb.Run(Get(Id::kWillPlay));
}

}  // namespace learning
}  // namespace media
