// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_task_queue.h"

#include <map>
#include <utility>

#include "components/webapps/browser/installable/installable_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

InstallableTaskQueue::InstallableTaskQueue() = default;

InstallableTaskQueue::~InstallableTaskQueue() = default;

void InstallableTaskQueue::Add(std::unique_ptr<InstallableTask> task) {
  tasks_.push_back(std::move(task));
}

void InstallableTaskQueue::PauseCurrent() {
  DCHECK(HasCurrent());
  paused_tasks_.push_back(std::move(tasks_.front()));
  Next();
}

void InstallableTaskQueue::UnpauseAll() {
  while (!paused_tasks_.empty()) {
    Add(std::move(paused_tasks_.front()));
    paused_tasks_.pop_front();
  }
}

bool InstallableTaskQueue::HasCurrent() const {
  return !tasks_.empty();
}

bool InstallableTaskQueue::HasPaused() const {
  return !paused_tasks_.empty();
}

InstallableTask& InstallableTaskQueue::Current() {
  DCHECK(HasCurrent());
  return *tasks_.front().get();
}

void InstallableTaskQueue::Next() {
  DCHECK(HasCurrent());
  tasks_.pop_front();
}

void InstallableTaskQueue::ResetWithError(InstallableStatusCode code) {
  auto tasks = std::move(tasks_);
  auto paused_tasks = std::move(paused_tasks_);
  for (auto& task : tasks) {
    task->ResetWithError(code);
  }
  for (auto& task : paused_tasks) {
    task->ResetWithError(code);
  }
}

}  // namespace webapps
