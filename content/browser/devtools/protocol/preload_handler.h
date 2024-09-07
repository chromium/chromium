// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/preload.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHostImpl;
class RenderFrameHostImpl;
struct PrerenderMismatchedHeaders;

namespace protocol {

class PreloadHandler : public DevToolsDomainHandler, public Preload::Backend {
 public:
  PreloadHandler();
  PreloadHandler(const PreloadHandler&) = delete;
  PreloadHandler& operator=(const PreloadHandler&) = delete;

  ~PreloadHandler() override;

  static std::vector<PreloadHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void DidUpdatePrefetchStatus(
      const base::UnguessableToken& initiator_devtools_navigation_token,
      const std::string& initiating_frame_id,
      const GURL& prefetch_url,
      PreloadingTriggeringOutcome status,
      PrefetchStatus prefetch_status,
      const std::string& request_id);
  void DidUpdatePrerenderStatus(
      const base::UnguessableToken& initiator_devtools_navigation_token,
      const GURL& prerender_url,
      std::optional<blink::mojom::SpeculationTargetHint> target_hint,
      PreloadingTriggeringOutcome status,
      std::optional<PrerenderFinalStatus> prerender_status,
      std::optional<std::string> disallowed_mojo_interface,
      const std::vector<PrerenderMismatchedHeaders>* mismatched_headers);

 private:
  Response Enable() override;
  Response Disable() override;

  void Wire(UberDispatcher* dispatcher) override;

  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;

  void SendInitialPreloadEnabledState();
  void SendCurrentPreloadStatus();

  raw_ptr<RenderFrameHostImpl> host_ = nullptr;

  bool enabled_ = false;

  std::unique_ptr<Preload::Frontend> frontend_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_PRELOAD_HANDLER_H_
