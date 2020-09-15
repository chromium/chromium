// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_AUDITS_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_AUDITS_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/audits.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class DevToolsAgentHostImpl;

namespace protocol {

class AuditsHandler final : public DevToolsDomainHandler,
                            public Audits::Backend {
 public:
  AuditsHandler();
  ~AuditsHandler() override;

  static std::vector<AuditsHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler implementation.
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // Audits::Backend implementation.
  DispatchResponse Disable() override;
  DispatchResponse Enable() override;

  void OnIssueAdded(const protocol::Audits::InspectorIssue* issue);

 private:
  std::unique_ptr<Audits::Frontend> frontend_;
  bool enabled_ = false;
  RenderFrameHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(AuditsHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_AUDITS_HANDLER_H_
