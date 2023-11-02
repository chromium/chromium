// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
#define MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "media/learning/common/learning_session.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/feature_provider.h"

namespace media {
namespace learning {

// Concrete implementation of a LearningSession.  This allows registration of
// learning tasks.
class COMPONENT_EXPORT(LEARNING_IMPL) LearningSessionImpl
    : public LearningSession {
 public:
  // We will create LearningTaskControllers that run on |task_runner|.
  explicit LearningSessionImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~LearningSessionImpl() override;

  // Create a SequenceBound controller for |task| on |task_runner|.
  using CreateTaskControllerCB =
      base::RepeatingCallback<base::SequenceBound<LearningTaskController>(
          scoped_refptr<base::SequencedTaskRunner>,
          const LearningTask&,
          SequenceBoundFeatureProvider)>;

  void SetTaskControllerFactoryCBForTesting(CreateTaskControllerCB cb);

  // LearningSession
  std::unique_ptr<LearningTaskController> GetController(
      const std::string& task_name) override;

  // Registers |task|, so that calls to AddExample with |task.name| will work.
  // This will create a new controller for the task.
  void RegisterTask(const LearningTask& task,
                    SequenceBoundFeatureProvider feature_provider =
                        SequenceBoundFeatureProvider());

 private:
  // Task runner on which we'll create controllers.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // [task_name] = task controller.
  using LearningTaskControllerMap =
      std::map<std::string, base::SequenceBound<LearningTaskController>>;
  LearningTaskControllerMap controller_map_;

  // Used to fetch registered LearningTasks from their name.
  std::map<std::string, LearningTask> task_map_;

  CreateTaskControllerCB controller_factory_;

  base::WeakPtrFactory<LearningSessionImpl> weak_factory_{this};
};

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_IMPL_LEARNING_SESSION_IMPL_H_
