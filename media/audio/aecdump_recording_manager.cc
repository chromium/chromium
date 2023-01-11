// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/aecdump_recording_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "media/audio/audio_manager.h"

namespace media {
namespace {
void CloseFileWithoutBlocking(base::File file) {
  // Post as a low-priority task to a thread pool to avoid blocking the
  // current thread.
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::TaskPriority::LOWEST, base::MayBlock()},
                             base::DoNothingWithBoundArgs(std::move(file)));
}
}  // namespace

AecdumpRecordingManager::AecdumpRecordingManager(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

AecdumpRecordingManager::~AecdumpRecordingManager() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(aecdump_recording_sources_.size(), 0u);
  DCHECK(!IsDebugRecordingEnabled());
}

void AecdumpRecordingManager::EnableDebugRecording(
    CreateFileCallback create_file_callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(create_file_callback);
  DCHECK(!create_file_callback_);
  create_file_callback_ = std::move(create_file_callback);

  for (const auto& it : aecdump_recording_sources_) {
    AecdumpRecordingSource* source = it.first;
    uint32_t id = it.second;
    create_file_callback_.Run(
        id, /*reply_callback=*/base::BindOnce(
            &StartRecordingIfValidPointer, weak_factory_.GetWeakPtr(), source));
  }
}

void AecdumpRecordingManager::StartRecording(AecdumpRecordingSource* source,
                                             base::File file) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(IsDebugRecordingEnabled());

  if (aecdump_recording_sources_.find(source) !=
      aecdump_recording_sources_.end()) {
    source->StartAecdump(std::move(file));
    return;
  }

  // The source is deregistered and we are responsible for closing the file.
  CloseFileWithoutBlocking(std::move(file));
}

void AecdumpRecordingManager::DisableDebugRecording() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(create_file_callback_);
  for (const auto& it : aecdump_recording_sources_) {
    AecdumpRecordingSource* source = it.first;
    source->StopAecdump();
  }
  create_file_callback_.Reset();

  // By invalidating weak pointers, we cancel any recordings in the process of
  // being started (file creation).
  weak_factory_.InvalidateWeakPtrs();
}

void AecdumpRecordingManager::RegisterAecdumpSource(
    AecdumpRecordingSource* source) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(aecdump_recording_sources_.find(source) ==
         aecdump_recording_sources_.end());

  const uint32_t id = recording_id_counter_++;

  if (IsDebugRecordingEnabled()) {
    create_file_callback_.Run(id, /*reply_callback=*/base::BindOnce(
                                  &AecdumpRecordingManager::StartRecording,
                                  weak_factory_.GetWeakPtr(), source));
  }
  aecdump_recording_sources_[source] = id;
}

void AecdumpRecordingManager::DeregisterAecdumpSource(
    AecdumpRecordingSource* source) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(aecdump_recording_sources_.find(source) !=
         aecdump_recording_sources_.end());

  if (IsDebugRecordingEnabled()) {
    source->StopAecdump();
  }
  aecdump_recording_sources_.erase(source);
}

bool AecdumpRecordingManager::IsDebugRecordingEnabled() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return !create_file_callback_.is_null();
}

// static
void AecdumpRecordingManager::StartRecordingIfValidPointer(
    base::WeakPtr<AecdumpRecordingManager> manager,
    AecdumpRecordingSource* source,
    base::File file) {
  if (manager) {
    manager->StartRecording(source, std::move(file));
    return;
  }
  // Recording has been stopped and we are responsible for closing the file.
  CloseFileWithoutBlocking(std::move(file));
}
}  // namespace media
