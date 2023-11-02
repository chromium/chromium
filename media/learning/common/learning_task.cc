// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/learning_task.h"

#include "base/hash/hash.h"
#include "base/no_destructor.h"

namespace media {
namespace learning {

LearningTask::LearningTask() = default;

LearningTask::LearningTask(
    const std::string& name,
    Model model,
    std::initializer_list<ValueDescription> feature_init_list,
    ValueDescription target_description)
    : name(name),
      model(model),
      feature_descriptions(std::move(feature_init_list)),
      target_description(target_description) {}

LearningTask::LearningTask(const LearningTask&) = default;

LearningTask::~LearningTask() = default;

LearningTask::Id LearningTask::GetId() const {
  return base::PersistentHash(name);
}

// static
const LearningTask& LearningTask::Empty() {
  static const base::NoDestructor<LearningTask> empty_task;
  return *empty_task;
}

}  // namespace learning
}  // namespace media
