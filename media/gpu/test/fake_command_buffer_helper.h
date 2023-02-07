// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_
#define MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_

#include <map>
#include <set>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/gpu/command_buffer_helper.h"

namespace media {

class FakeCommandBufferHelper : public CommandBufferHelper {
 public:
  explicit FakeCommandBufferHelper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  FakeCommandBufferHelper(const FakeCommandBufferHelper&) = delete;
  FakeCommandBufferHelper& operator=(const FakeCommandBufferHelper&) = delete;

  void WaitForSyncToken(gpu::SyncToken sync_token,
                        base::OnceClosure done_cb) override;
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

#if !BUILDFLAG(IS_ANDROID)
  // CommandBufferHelper implementation.
  gl::GLContext* GetGLContext() override;
  gpu::SharedImageStub* GetSharedImageStub() override;
#if BUILDFLAG(IS_WIN)
  gpu::DXGISharedHandleManager* GetDXGISharedHandleManager() override;
#endif

  gpu::MemoryTypeTracker* GetMemoryTypeTracker() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  bool HasStub() override;
  bool MakeContextCurrent() override;
  std::unique_ptr<gpu::SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<gpu::SharedImageBacking> backing) override;
  gpu::TextureBase* GetTexture(GLuint service_id) const override;
  GLuint CreateTexture(GLenum target,
                       GLenum internal_format,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type) override;
  void DestroyTexture(GLuint service_id) override;
  void SetCleared(GLuint service_id) override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
  bool BindDecoderManagedImage(GLuint service_id, gl::GLImage* image) override;
#else
  bool BindClientManagedImage(GLuint service_id, gl::GLImage* image) override;
#endif
  gpu::Mailbox CreateLegacyMailbox(GLuint service_id) override;
  void AddWillDestroyStubCB(WillDestroyStubCB callback) override;
  bool IsPassthrough() const override;
  bool SupportsTextureRectangle() const override;
#endif

 private:
  ~FakeCommandBufferHelper() override;

  bool BindImageInternal(GLuint service_id, gl::GLImage* image);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool has_stub_ = true;
  bool is_context_lost_ = false;
  bool is_context_current_ = false;

  GLuint next_service_id_ = 1;
  std::set<GLuint> service_ids_;
  std::map<gpu::SyncToken, base::OnceClosure> waits_;

  std::vector<WillDestroyStubCB> will_destroy_stub_callbacks_;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_
