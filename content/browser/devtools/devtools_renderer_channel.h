// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"

namespace gfx {
class Point;
}

namespace content {

class DevToolsAgentHostImpl;
class DevToolsSession;
class RenderFrameHostImpl;

// This class encapsulates a connection to blink::mojom::DevToolsAgent
// in the renderer.
// When the renderer changes, different DevToolsAgentHostImpl subclasses
// retrieve a new blink::mojom::DevToolsAgent pointer, and this channel
// starts using it for all existing and future sessions.
class CONTENT_EXPORT DevToolsRendererChannel
    : public blink::mojom::DevToolsAgentHost {
 public:
  explicit DevToolsRendererChannel(DevToolsAgentHostImpl* owner);
  ~DevToolsRendererChannel() override;

  void SetRenderer(
      blink::mojom::DevToolsAgentAssociatedPtr agent_ptr,
      blink::mojom::DevToolsAgentHostAssociatedRequest host_request,
      int process_id,
      RenderFrameHostImpl* frame_host);
  void AttachSession(DevToolsSession* session);
  void InspectElement(const gfx::Point& point);

 private:
  DevToolsAgentHostImpl* owner_;
  mojo::AssociatedBinding<blink::mojom::DevToolsAgentHost> binding_;
  blink::mojom::DevToolsAgentAssociatedPtr agent_ptr_;
  int process_id_;
  RenderFrameHostImpl* frame_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DevToolsRendererChannel);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
