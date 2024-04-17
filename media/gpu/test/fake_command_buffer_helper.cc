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

void FakeCommandBufferHelper::StubLost() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Keep a reference to |this| in case the destruction cb drops the last one.
  scoped_refptr<CommandBufferHelper> thiz(this);
  for (auto& callback : will_destroy_stub_callbacks_) {
    std::move(callback).Run(!is_context_lost_);
  }
  has_stub_ = false;
  is_context_lost_ = true;
  is_context_current_ = false;
  waits_.clear();
}

void FakeCommandBufferHelper::ContextLost() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_lost_ = true;
  is_context_current_ = false;
}

void FakeCommandBufferHelper::CurrentContextLost() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_current_ = false;
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
  if (has_stub_) {
    waits_.emplace(sync_token, std::move(done_cb));
  }
}

#if !BUILDFLAG(IS_ANDROID)
gl::GLContext* FakeCommandBufferHelper::GetGLContext() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  return nullptr;
}

gpu::SharedImageStub* FakeCommandBufferHelper::GetSharedImageStub() {
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
gpu::DXGISharedHandleManager*
FakeCommandBufferHelper::GetDXGISharedHandleManager() {
  return nullptr;
}
#endif

gpu::MemoryTypeTracker* FakeCommandBufferHelper::GetMemoryTypeTracker() {
  return nullptr;
}

gpu::SharedImageManager* FakeCommandBufferHelper::GetSharedImageManager() {
  return nullptr;
}

bool FakeCommandBufferHelper::HasStub() {
  return has_stub_;
}

bool FakeCommandBufferHelper::MakeContextCurrent() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_current_ = !is_context_lost_;
  return is_context_current_;
}

std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>
FakeCommandBufferHelper::Register(
    std::unique_ptr<gpu::SharedImageBacking> backing) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  return nullptr;
}

void FakeCommandBufferHelper::AddWillDestroyStubCB(WillDestroyStubCB callback) {
  will_destroy_stub_callbacks_.push_back(std::move(callback));
}

bool FakeCommandBufferHelper::SupportsTextureRectangle() const {
  return false;
}
#endif

}  // namespace media
