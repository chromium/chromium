// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fed_cm.h"
#include "content/common/content_export.h"

namespace content {
class FederatedAuthRequestPageData;
}  // namespace content

namespace content::protocol {

class FedCmHandler : public DevToolsDomainHandler, public FedCm::Backend {
 public:
  CONTENT_EXPORT FedCmHandler();
  CONTENT_EXPORT ~FedCmHandler() override;
  FedCmHandler(const FedCmHandler&) = delete;
  FedCmHandler operator=(const FedCmHandler&) = delete;

  static std::vector<FedCmHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void WillShowDialog(bool* intercept) {
    if (enabled_) {
      *intercept = true;
    }
  }
  void OnDialogShown();

 private:
  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // FedCm::Backend
  DispatchResponse Enable() override;
  DispatchResponse Disable() override;

  FederatedAuthRequestPageData* GetPageData();

  RenderFrameHostImpl* frame_host_ = nullptr;
  std::unique_ptr<FedCm::Frontend> frontend_;
  bool enabled_ = false;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_
