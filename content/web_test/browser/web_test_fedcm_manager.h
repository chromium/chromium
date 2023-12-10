// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_FEDCM_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_FEDCM_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request_automation.mojom.h"

namespace content {

class FederatedAuthRequestImpl;
class RenderFrameHost;
class RenderFrameHostImpl;

class WebTestFedCmManager
    : public blink::test::mojom::FederatedAuthRequestAutomation {
 public:
  explicit WebTestFedCmManager(RenderFrameHost* render_frame_host);

  WebTestFedCmManager(const WebTestFedCmManager&) = delete;
  WebTestFedCmManager& operator=(const WebTestFedCmManager&) = delete;

  ~WebTestFedCmManager() override;

  // blink::test::mojom::FederatedAuthRequestAutomation
  void GetDialogType(
      blink::test::mojom::FederatedAuthRequestAutomation::GetDialogTypeCallback)
      override;
  void GetFedCmDialogTitle(blink::test::mojom::FederatedAuthRequestAutomation::
                               GetFedCmDialogTitleCallback) override;
  void SelectFedCmAccount(uint32_t account_index,
                          SelectFedCmAccountCallback) override;
  void DismissFedCmDialog(DismissFedCmDialogCallback) override;
  void ClickFedCmDialogButton(blink::test::mojom::DialogButton button,
                              ClickFedCmDialogButtonCallback) override;

 private:
  // Returns the active FederatedAuthRequestImpl for the current Page,
  // or nullptr if there isn't one.
  FederatedAuthRequestImpl* GetAuthRequestImpl();

  base::WeakPtr<RenderFrameHostImpl> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_FEDCM_MANAGER_H_
