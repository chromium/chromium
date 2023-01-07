// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/webid_test_content_browser_client.h"

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

}  // namespace content
