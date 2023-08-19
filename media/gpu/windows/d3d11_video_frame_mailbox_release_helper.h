// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_FRAME_MAILBOX_RELEASE_HELPER_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_FRAME_MAILBOX_RELEASE_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace media {

class MediaLog;

// Waits for SyncTokens during mailbox release for D3D11VideoDecoder frames. May
// only be used on the GPU main thread. May destruct on any thread.
class MEDIA_GPU_EXPORT D3D11VideoFrameMailboxReleaseHelper
    : public base::RefCountedThreadSafe<D3D11VideoFrameMailboxReleaseHelper> {
 public:
  // May be constructed on any thread.
  D3D11VideoFrameMailboxReleaseHelper(
      std::unique_ptr<MediaLog> media_log,
      base::OnceCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb);

  D3D11VideoFrameMailboxReleaseHelper(
      const D3D11VideoFrameMailboxReleaseHelper&) = delete;
  D3D11VideoFrameMailboxReleaseHelper& operator=(
      const D3D11VideoFrameMailboxReleaseHelper&) = delete;

  // Callback to us to wait for a sync token, then call a closure.
  using ReleaseMailboxCB =
      base::RepeatingCallback<void(base::OnceClosure, const gpu::SyncToken&)>;
  using InitCB = base::OnceCallback<void(bool success, ReleaseMailboxCB)>;

  // We will call back |init_cb| with the init status.
  void Initialize(InitCB init_cb);

 private:
  friend class base::RefCountedThreadSafe<D3D11VideoFrameMailboxReleaseHelper>;

  virtual ~D3D11VideoFrameMailboxReleaseHelper();

  // Called to wait on |sync_token|, and call |wait_complete_cb| when done.
  void OnMailboxReleased(base::OnceClosure wait_complete_cb,
                         const gpu::SyncToken& sync_token);

  void OnSyncTokenReleased(base::OnceClosure);

  std::unique_ptr<MediaLog> media_log_;

  base::OnceCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb_;
  scoped_refptr<CommandBufferHelper> helper_;

  // Has thread affinity -- must be run on the gpu main thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_FRAME_MAILBOX_RELEASE_HELPER_H_
