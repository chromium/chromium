// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/context_menu_interceptor.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

ContextMenuInterceptor::ContextMenuInterceptor(
    content::RenderFrameHost* render_frame_host,
    ShowBehavior behavior)
    : swapped_impl_(RenderFrameHostImpl::From(render_frame_host)
                        ->local_frame_host_receiver_for_testing(),
                    this),
      run_loop_(std::make_unique<base::RunLoop>()),
      quit_closure_(run_loop_->QuitClosure()),
      show_behavior_(behavior) {}

ContextMenuInterceptor::~ContextMenuInterceptor() = default;

void ContextMenuInterceptor::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  run_loop_->Run();
  run_loop_ = nullptr;
}

void ContextMenuInterceptor::Reset() {
  ASSERT_EQ(run_loop_, nullptr);
  run_loop_ = std::make_unique<base::RunLoop>();
  quit_closure_ = run_loop_->QuitClosure();
}

blink::mojom::LocalFrameHost* ContextMenuInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}

void ContextMenuInterceptor::ShowContextMenu(
    mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
        context_menu_client,
    const blink::UntrustworthyContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  last_params_ = params;
  std::move(quit_closure_).Run();

  if (show_behavior_ == ShowBehavior::kPreventShow) {
    return;
  }

  GetForwardingInterface()->ShowContextMenu(std::move(context_menu_client),
                                            params);
}

}  // namespace content
