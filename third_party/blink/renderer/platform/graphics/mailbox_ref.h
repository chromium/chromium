// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_REF_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_REF_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace viz {
class SingleReleaseCallback;
}  // namespace viz

namespace blink {
class WebGraphicsContext3DProviderWrapper;

class MailboxRef : public ThreadSafeRefCounted<MailboxRef> {
 public:
  MailboxRef(const gpu::SyncToken& sync_token,
             base::PlatformThreadRef context_thread_ref,
             scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
             std::unique_ptr<viz::SingleReleaseCallback> release_callback);
  ~MailboxRef();

  bool is_cross_thread() const {
    return base::PlatformThread::CurrentRef() != context_thread_ref_;
  }
  void set_sync_token(gpu::SyncToken token) {
    DCHECK(sync_token_.HasData());
    sync_token_ = token;
  }
  const gpu::SyncToken& sync_token() const { return sync_token_; }
  bool verified_flush() { return sync_token_.verified_flush(); }

 private:
  gpu::SyncToken sync_token_;
  const base::PlatformThreadRef context_thread_ref_;
  const scoped_refptr<base::SingleThreadTaskRunner> context_task_runner_;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
};

}  // namespace blink

#endif
