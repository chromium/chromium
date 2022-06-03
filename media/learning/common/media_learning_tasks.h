// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_MEDIA_LEARNING_TASKS_H_
#define MEDIA_LEARNING_COMMON_MEDIA_LEARNING_TASKS_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "media/learning/common/learning_task.h"

namespace media {
namespace learning {

namespace tasknames {
constexpr char kWillPlay[] = "MediaLearningWillPlay";
constexpr char kConsecutiveBadWindows[] = "MediaLearningConsecutiveBadWindows";
constexpr char kConsecutiveNNRs[] = "MediaLearningConsecutiveNNRs";
}  // namespace tasknames

// All learning experiments for media/ .
// TODO(liberato): This should be in media/ somewhere, since the learning
// framework doesn't care about it.  For now, this is simpler to make deps
// easier to handle.
class COMPONENT_EXPORT(LEARNING_COMMON) MediaLearningTasks {
 public:
  // Return the LearningTask for |name|.
  static const learning::LearningTask& Get(const char* task_name);

  // Register all tasks by calling |registration_cb| repeatedly.
  static void Register(
      base::RepeatingCallback<void(const learning::LearningTask&)>
          registration_cb);

 private:
  MediaLearningTasks() = delete;
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_MEDIA_LEARNING_TASKS_H_
