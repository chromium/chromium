// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MESSAGING_CHANNEL_ENDPOINT_H_
#define EXTENSIONS_BROWSER_API_MESSAGING_CHANNEL_ENDPOINT_H_

#include "extensions/browser/service_worker/worker_id.h"

#include "extensions/common/api/messaging/port_context.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace extensions {

// Represents an endpoint (tab, frame or worker) of a message channel in a
// render process.
// TODO(crbug.com/939594): Consolidate all classes/structs around extension
// message ports.
class ChannelEndpoint {
 public:
  // An endpoint for a PortContext.
  ChannelEndpoint(content::BrowserContext* browser_context,
                  int render_process_id,
                  const PortContext& port_context);
  // An endpoint for a native message host.
  ChannelEndpoint(content::BrowserContext* browser_context);

  content::BrowserContext* browser_context() const { return browser_context_; }
  int render_process_id() const { return render_process_id_; }
  const PortContext& port_context() const { return port_context_; }

  bool is_for_service_worker() const;
  bool is_for_render_frame() const;
  bool is_for_native_host() const;

  // If the endpoint is a Service Worker, returns its worker id.
  // Only valid for worker endpoint.
  WorkerId GetWorkerId() const;

  // Returns the render frame if this endpoint points to a frame. Returns
  // nullptr if the frame is no longer alive.
  // Only valid for frame endpoint.
  content::RenderFrameHost* GetRenderFrameHost() const;

  // Returns whether the endpoint is currently live.
  bool IsValid() const;

 private:
  content::BrowserContext* const browser_context_;
  const int render_process_id_;
  const PortContext port_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MESSAGING_CHANNEL_ENDPOINT_H_
