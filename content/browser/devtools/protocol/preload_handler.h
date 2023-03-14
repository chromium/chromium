// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/preload.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHostImpl;
class NavigationRequest;
class RenderFrameHostImpl;

namespace protocol {

class PreloadHandler : public DevToolsDomainHandler, public Preload::Backend {
 public:
  PreloadHandler();
  PreloadHandler(const PreloadHandler&) = delete;
  PreloadHandler& operator=(const PreloadHandler&) = delete;

  ~PreloadHandler() override;

  static std::vector<PreloadHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void DidActivatePrerender(const NavigationRequest& nav_request);
  void DidCancelPrerender(const GURL& prerendering_url,
                          const std::string& initiating_frame_id,
                          PrerenderFinalStatus status,
                          const std::string& disallowed_api_method);

  void DidUpdatePrefetchStatus(const std::string& initiating_frame_id,
                               const GURL& prefetch_url,
                               PreloadingTriggeringOutcome status);
  void DidUpdatePrerenderStatus(const std::string& initiating_frame_id,
                                const GURL& prerender_url,
                                PreloadingTriggeringOutcome status);

 private:
  Response Enable() override;
  Response Disable() override;

  void Wire(UberDispatcher* dispatcher) override;

  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  void RetrievePrerenderActivationFromWebContents();

  RenderFrameHostImpl* host_ = nullptr;

  // Whether stored prerender activation has been dispatched to Devtools. Reset
  // whenever a new prerender event received.
  bool has_dispatched_stored_prerender_activation_ = false;

  bool enabled_ = false;

  std::unique_ptr<Preload::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_
