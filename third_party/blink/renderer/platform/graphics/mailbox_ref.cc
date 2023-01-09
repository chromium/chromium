// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"

#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

MailboxRef::MailboxRef(
    const gpu::SyncToken& sync_token,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    viz::ReleaseCallback release_callback)
    : sync_token_(sync_token),
      context_thread_ref_(context_thread_ref),
      context_task_runner_(std::move(context_task_runner)),
      release_callback_(std::move(release_callback)) {}

MailboxRef::~MailboxRef() {
  if (context_thread_ref_ == base::PlatformThread::CurrentRef()) {
    std::move(release_callback_).Run(sync_token_, /*is_lost=*/false);
  } else {
    context_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(release_callback_), sync_token_,
                                  /*is_lost=*/false));
  }
}

}  // namespace blink
