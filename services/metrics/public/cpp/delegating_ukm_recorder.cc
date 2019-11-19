// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#include "base/bind.h"
#include "base/lazy_instance.h"

namespace ukm {

namespace {

base::LazyInstance<DelegatingUkmRecorder>::Leaky g_ukm_recorder =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DelegatingUkmRecorder::DelegatingUkmRecorder() = default;
DelegatingUkmRecorder::~DelegatingUkmRecorder() = default;

// static
DelegatingUkmRecorder* DelegatingUkmRecorder::Get() {
  return &g_ukm_recorder.Get();
}

void DelegatingUkmRecorder::AddDelegate(base::WeakPtr<UkmRecorder> delegate) {
  base::AutoLock auto_lock(lock_);
  delegates_.insert(
      {delegate.get(),
       Delegate(base::SequencedTaskRunnerHandle::Get(), delegate)});
}

void DelegatingUkmRecorder::RemoveDelegate(UkmRecorder* delegate) {
  base::AutoLock auto_lock(lock_);
  delegates_.erase(delegate);
}

void DelegatingUkmRecorder::UpdateSourceURL(SourceId source_id,
                                            const GURL& url) {
  if (GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID ||
      GetSourceIdType(source_id) == SourceIdType::APP_ID) {
    DLOG(FATAL)
        << "UpdateSourceURL invoked for NAVIGATION_ID or APP_ID SourceId";
    return;
  }

  base::AutoLock auto_lock(lock_);
  for (auto& iterator : delegates_)
    iterator.second.UpdateSourceURL(source_id, url);
}

void DelegatingUkmRecorder::RecordNavigation(
    SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  if (GetSourceIdType(source_id) != SourceIdType::NAVIGATION_ID) {
    DLOG(FATAL) << "UpdateNavigationURL invoked for non-NAVIGATION_ID SourceId";
    return;
  }

  base::AutoLock auto_lock(lock_);
  for (auto& iterator : delegates_) {
    iterator.second.RecordNavigation(source_id, navigation_data);
  }
}

void DelegatingUkmRecorder::UpdateAppURL(SourceId source_id, const GURL& url) {
  if (GetSourceIdType(source_id) != SourceIdType::APP_ID) {
    DLOG(FATAL) << "UpdateAppURL invoked for non-APP_ID SourceId";
    return;
  }
  base::AutoLock auto_lock(lock_);
  for (auto& iterator : delegates_)
    iterator.second.UpdateAppURL(source_id, url);
}

void DelegatingUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  base::AutoLock auto_lock(lock_);
  // If there is exactly one delegate, just forward the call.
  if (delegates_.size() == 1) {
    delegates_.begin()->second.AddEntry(std::move(entry));
    return;
  }
  // Otherwise, make a copy for each delegate.
  for (auto& iterator : delegates_)
    iterator.second.AddEntry(entry->Clone());
}

void DelegatingUkmRecorder::MarkSourceForDeletion(ukm::SourceId source_id) {
  base::AutoLock auto_lock(lock_);
  for (auto& iterator : delegates_)
    iterator.second.MarkSourceForDeletion(source_id);
}

DelegatingUkmRecorder::Delegate::Delegate(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<UkmRecorder> ptr)
    : task_runner_(task_runner), ptr_(ptr) {}

DelegatingUkmRecorder::Delegate::Delegate(const Delegate& other)
    : task_runner_(other.task_runner_), ptr_(other.ptr_) {}

DelegatingUkmRecorder::Delegate::~Delegate() = default;

void DelegatingUkmRecorder::Delegate::UpdateSourceURL(ukm::SourceId source_id,
                                                      const GURL& url) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    ptr_->UpdateSourceURL(source_id, url);
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmRecorder::UpdateSourceURL, ptr_, source_id, url));
}

void DelegatingUkmRecorder::Delegate::UpdateAppURL(ukm::SourceId source_id,
                                                   const GURL& url) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    ptr_->UpdateAppURL(source_id, url);
    return;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&UkmRecorder::UpdateAppURL,
                                                   ptr_, source_id, url));
}

void DelegatingUkmRecorder::Delegate::RecordNavigation(
    ukm::SourceId source_id,
    const UkmSource::NavigationData& navigation_data) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    ptr_->RecordNavigation(source_id, navigation_data);
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmRecorder::RecordNavigation, ptr_, source_id,
                                navigation_data));
}

void DelegatingUkmRecorder::Delegate::AddEntry(mojom::UkmEntryPtr entry) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    ptr_->AddEntry(std::move(entry));
    return;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&UkmRecorder::AddEntry, ptr_,
                                                   std::move(entry)));
}

void DelegatingUkmRecorder::Delegate::MarkSourceForDeletion(
    ukm::SourceId source_id) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    ptr_->MarkSourceForDeletion(source_id);
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmRecorder::MarkSourceForDeletion, ptr_, source_id));
}

}  // namespace ukm
