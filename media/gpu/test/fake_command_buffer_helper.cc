// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/fake_command_buffer_helper.h"

#include "base/logging.h"

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
  if (will_destroy_stub_cb_)
    std::move(will_destroy_stub_cb_).Run(!is_context_lost_);
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

bool FakeCommandBufferHelper::HasTexture(GLuint service_id) {
  DVLOG(4) << __func__ << "(" << service_id << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  return service_ids_.count(service_id);
}

void FakeCommandBufferHelper::ReleaseSyncToken(gpu::SyncToken sync_token) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(waits_.count(sync_token));
  task_runner_->PostTask(FROM_HERE, std::move(waits_[sync_token]));
  waits_.erase(sync_token);
}

gl::GLContext* FakeCommandBufferHelper::GetGLContext() {
  DVLOG(4) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
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

GLuint FakeCommandBufferHelper::CreateTexture(GLenum target,
                                              GLenum internal_format,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(is_context_current_);
  GLuint service_id = next_service_id_++;
  service_ids_.insert(service_id);
  return service_id;
}

void FakeCommandBufferHelper::DestroyTexture(GLuint service_id) {
  DVLOG(2) << __func__ << "(" << service_id << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(is_context_current_);
  DCHECK(service_ids_.count(service_id));
  service_ids_.erase(service_id);
}

void FakeCommandBufferHelper::SetCleared(GLuint service_id) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
}

bool FakeCommandBufferHelper::BindImage(GLuint service_id,
                                        gl::GLImage* image,
                                        bool client_managed) {
  DVLOG(2) << __func__ << "(" << service_id << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
  return has_stub_;
}

gpu::Mailbox FakeCommandBufferHelper::CreateMailbox(GLuint service_id) {
  DVLOG(2) << __func__ << "(" << service_id << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
  if (!has_stub_)
    return gpu::Mailbox();
  return gpu::Mailbox::Generate();
}

void FakeCommandBufferHelper::ProduceTexture(const gpu::Mailbox& mailbox,
                                             GLuint service_id) {
  DVLOG(2) << __func__ << "(" << service_id << ")";
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
}

void FakeCommandBufferHelper::WaitForSyncToken(gpu::SyncToken sync_token,
                                               base::OnceClosure done_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!waits_.count(sync_token));
  if (has_stub_)
    waits_.emplace(sync_token, std::move(done_cb));
}

void FakeCommandBufferHelper::SetWillDestroyStubCB(
    WillDestroyStubCB will_destroy_stub_cb) {
  DCHECK(!will_destroy_stub_cb_);
  will_destroy_stub_cb_ = std::move(will_destroy_stub_cb);
}

}  // namespace media
