// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/fake_command_buffer_helper.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace media {

FakeCommandBufferHelper::FakeCommandBufferHelper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : CommandBufferHelper(task_runner), task_runner_(std::move(task_runner)) {
  DVLOG(1) << __func__;
}

FakeCommandBufferHelper::~FakeCommandBufferHelper() {
  DVLOG(1) << __func__;
}

void FakeCommandBufferHelper::ReleaseSyncToken(gpu::SyncToken sync_token) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(waits_.count(sync_token));
  task_runner_->PostTask(FROM_HERE, std::move(waits_[sync_token]));
  waits_.erase(sync_token);
}

void FakeCommandBufferHelper::WaitForSyncToken(gpu::SyncToken sync_token,
                                               base::OnceClosure done_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!waits_.count(sync_token));
  waits_.emplace(sync_token, std::move(done_cb));
}

#if !BUILDFLAG(IS_ANDROID)
gpu::SharedImageStub* FakeCommandBufferHelper::GetSharedImageStub() {
  return nullptr;
}

gpu::MemoryTypeTracker* FakeCommandBufferHelper::GetMemoryTypeTracker() {
  return nullptr;
}

gpu::SharedImageManager* FakeCommandBufferHelper::GetSharedImageManager() {
  return nullptr;
}

#endif

}  // namespace media
