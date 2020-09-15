// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/idle_test_utils.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/idle_manager.h"

namespace content {

void IdleManagerHelper::SetIdleTimeProviderForTest(
    content::RenderFrameHost* frame,
    std::unique_ptr<IdleManager::IdleTimeProvider> idle_time_provider) {
  content::RenderFrameHostImpl* const frame_impl =
      static_cast<content::RenderFrameHostImpl*>(frame);

  content::IdleManager* idle_mgr = frame_impl->GetIdleManager();

  idle_mgr->SetIdleTimeProviderForTest(std::move(idle_time_provider));
}
}  // namespace content
