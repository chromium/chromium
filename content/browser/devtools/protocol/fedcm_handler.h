// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/fed_cm.h"
#include "content/common/content_export.h"

namespace content {
class FederatedAuthRequestImpl;
class FederatedAuthRequestPageData;
class FederatedIdentityApiPermissionContextDelegate;
class IdentityProviderData;
class IdentityRequestAccount;
}  // namespace content

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;

namespace content::protocol {

class FedCmHandler : public DevToolsDomainHandler, public FedCm::Backend {
 public:
  CONTENT_EXPORT FedCmHandler();
  CONTENT_EXPORT ~FedCmHandler() override;
  FedCmHandler(const FedCmHandler&) = delete;
  FedCmHandler operator=(const FedCmHandler&) = delete;

  static std::vector<FedCmHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  void WillSendRequest(bool* intercept, bool* disable_delay) {
    if (enabled_) {
      *intercept = true;
      *disable_delay |= disable_delay_;
    }
  }

  void WillShowDialog(bool* intercept) {
    if (enabled_) {
      *intercept = true;
    }
  }
  void DidShowDialog();
  void DidCloseDialog();

 private:
  // DevToolsDomainHandler:
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  void Wire(UberDispatcher* dispatcher) override;

  // FedCm::Backend
  DispatchResponse Enable(Maybe<bool> in_disableRejectionDelay) override;
  DispatchResponse Disable() override;
  DispatchResponse SelectAccount(const String& in_dialogId,
                                 int in_accountIndex) override;
  DispatchResponse OpenUrl(
      const String& in_dialogId,
      int in_accountIndex,
      const FedCm::AccountUrlType& in_accountUrlType) override;
  DispatchResponse ClickDialogButton(
      const String& in_dialogId,
      const FedCm::DialogButton& in_dialogButton) override;
  DispatchResponse DismissDialog(const String& in_dialogId,
                                 Maybe<bool> in_triggerCooldown) override;
  DispatchResponse ResetCooldown() override;

  url::Origin GetEmbeddingOrigin();

  FederatedAuthRequestPageData* GetPageData();
  FederatedAuthRequestImpl* GetFederatedAuthRequest();
  const std::vector<IdentityProviderDataPtr>* GetIdentityProviderData(
      FederatedAuthRequestImpl* auth_request);
  const std::vector<IdentityRequestAccountPtr>* GetAccounts(
      FederatedAuthRequestImpl* auth_request);
  FederatedIdentityApiPermissionContextDelegate* GetApiPermissionContext();

  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  std::unique_ptr<FedCm::Frontend> frontend_;
  std::string dialog_id_;
  bool enabled_ = false;
  bool disable_delay_ = false;
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FEDCM_HANDLER_H_
