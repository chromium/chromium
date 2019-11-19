// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_
#define MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "media/gpu/command_buffer_helper.h"

namespace media {

class FakeCommandBufferHelper : public CommandBufferHelper {
 public:
  explicit FakeCommandBufferHelper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Signal stub destruction. All textures will be deleted.  Listeners will
  // be notified that we have a current context unless one calls ContextLost
  // before this.
  void StubLost();

  // Signal context loss. MakeContextCurrent() fails after this.
  void ContextLost();

  // Signal that the context is no longer current.
  void CurrentContextLost();

  // Complete a pending SyncToken wait.
  void ReleaseSyncToken(gpu::SyncToken sync_token);

  // Test whether a texture exists (has not been destroyed).
  bool HasTexture(GLuint service_id);

  // CommandBufferHelper implementation.
  gl::GLContext* GetGLContext() override;
  bool HasStub() override;
  bool MakeContextCurrent() override;
  GLuint CreateTexture(GLenum target,
                       GLenum internal_format,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type) override;
  void DestroyTexture(GLuint service_id) override;
  void SetCleared(GLuint service_id) override;
  bool BindImage(GLuint service_id,
                 gl::GLImage* image,
                 bool client_managed) override;
  gpu::Mailbox CreateMailbox(GLuint service_id) override;
  void ProduceTexture(const gpu::Mailbox& mailbox, GLuint service_id) override;
  void WaitForSyncToken(gpu::SyncToken sync_token,
                        base::OnceClosure done_cb) override;
  void SetWillDestroyStubCB(WillDestroyStubCB will_destroy_stub_cb) override;

 private:
  ~FakeCommandBufferHelper() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool has_stub_ = true;
  bool is_context_lost_ = false;
  bool is_context_current_ = false;

  GLuint next_service_id_ = 1;
  std::set<GLuint> service_ids_;
  std::map<gpu::SyncToken, base::OnceClosure> waits_;

  WillDestroyStubCB will_destroy_stub_cb_;

  DISALLOW_COPY_AND_ASSIGN(FakeCommandBufferHelper);
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_
