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

#if !BUILDFLAG(IS_ANDROID)
  // CommandBufferHelper implementation.
  gpu::SharedImageStub* GetSharedImageStub() override;
  gpu::MemoryTypeTracker* GetMemoryTypeTracker() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
#endif

 private:
  ~FakeCommandBufferHelper() override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::map<gpu::SyncToken, base::OnceClosure> waits_;
};

}  // namespace media

#endif  // MEDIA_GPU_TEST_FAKE_COMMAND_BUFFER_HELPER_H_
