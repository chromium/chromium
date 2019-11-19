// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/channel_endpoint.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

namespace extensions {

ChannelEndpoint::ChannelEndpoint(content::BrowserContext* browser_context,
                                 int render_process_id,
                                 const PortContext& port_context)
    : browser_context_(browser_context),
      render_process_id_(render_process_id),
      port_context_(port_context) {
  // Context must be exclusive to render frame or worker.
  DCHECK_NE(port_context.is_for_service_worker(),
            port_context.is_for_render_frame());
}

// For native message endpoint.
ChannelEndpoint::ChannelEndpoint(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID) {
  DCHECK(!port_context_.is_for_render_frame() &&
         !port_context_.is_for_service_worker());
}

bool ChannelEndpoint::is_for_service_worker() const {
  return port_context_.is_for_service_worker();
}

bool ChannelEndpoint::is_for_render_frame() const {
  return port_context_.frame.has_value();
}

bool ChannelEndpoint::is_for_native_host() const {
  return !port_context_.is_for_render_frame() &&
         !port_context_.is_for_service_worker();
}

content::RenderFrameHost* ChannelEndpoint::GetRenderFrameHost() const {
  DCHECK(port_context_.is_for_render_frame());
  return content::RenderFrameHost::FromID(render_process_id_,
                                          port_context_.frame->routing_id);
}

WorkerId ChannelEndpoint::GetWorkerId() const {
  DCHECK(port_context_.is_for_service_worker());
  return {port_context_.worker->extension_id, render_process_id_,
          port_context_.worker->version_id, port_context_.worker->thread_id};
}

bool ChannelEndpoint::IsValid() const {
  if (is_for_service_worker()) {
    return ProcessManager::Get(browser_context())
        ->HasServiceWorker(GetWorkerId());
  }

  if (is_for_render_frame())
    return GetRenderFrameHost() != nullptr;

  DCHECK(is_for_native_host());
  return true;
}

}  // namespace extensions
