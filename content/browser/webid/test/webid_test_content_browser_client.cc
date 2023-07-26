// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/webid_test_content_browser_client.h"

#include "content/browser/webid/mdocs/mdoc_provider.h"
#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"

namespace content {

WebIdTestContentBrowserClient::WebIdTestContentBrowserClient() = default;
WebIdTestContentBrowserClient::~WebIdTestContentBrowserClient() = default;

std::unique_ptr<IdentityRequestDialogController>
WebIdTestContentBrowserClient::CreateIdentityRequestDialogController() {
  DCHECK(test_dialog_controller_);
  return std::move(test_dialog_controller_);
}

void WebIdTestContentBrowserClient::SetIdentityRequestDialogController(
    std::unique_ptr<IdentityRequestDialogController> controller) {
  test_dialog_controller_ = std::move(controller);
}

std::unique_ptr<MDocProvider>
WebIdTestContentBrowserClient::CreateMDocProvider() {
  DCHECK(test_mdoc_provider_);
  return std::move(test_mdoc_provider_);
}

void WebIdTestContentBrowserClient::SetMDocProvider(
    std::unique_ptr<MDocProvider> provider) {
  test_mdoc_provider_ = std::move(provider);
}

void WebIdTestContentBrowserClient::SetIdentityRegistry(
    WebContents* web_contents,
    FederatedIdentityModalDialogViewDelegate* delegate,
    const url::Origin& url) {
  IdentityRegistry::CreateForWebContents(web_contents, delegate, url);
}

}  // namespace content
