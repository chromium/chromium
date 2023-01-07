// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/identity_request_dialog_controller.h"

namespace content {

// Implements ContentBrowserClient to allow calls out to the Chrome layer to
// be stubbed for tests.
class WebIdTestContentBrowserClient : public ContentBrowserClient {
 public:
  WebIdTestContentBrowserClient();
  ~WebIdTestContentBrowserClient() override;

  WebIdTestContentBrowserClient(const WebIdTestContentBrowserClient&) = delete;
  WebIdTestContentBrowserClient& operator=(
      const WebIdTestContentBrowserClient&) = delete;

  std::unique_ptr<IdentityRequestDialogController>
  CreateIdentityRequestDialogController() override;

  // This needs to be called once for every WebID invocation. If there is a
  // need in future to generate these in sequence then a callback can be used.
  void SetIdentityRequestDialogController(
      std::unique_ptr<IdentityRequestDialogController> controller);

 private:
  std::unique_ptr<IdentityRequestDialogController> test_dialog_controller_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_
