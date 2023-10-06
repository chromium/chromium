// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "content/browser/webid/identity_registry.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/content_browser_test_content_browser_client.h"

namespace content {

class DigitalCredentialProvider;
class FederatedIdentityModalDialogViewDelegate;

// Implements ContentBrowserClient to allow calls out to the Chrome layer to
// be stubbed for tests.
class WebIdTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  WebIdTestContentBrowserClient();
  ~WebIdTestContentBrowserClient() override;

  WebIdTestContentBrowserClient(const WebIdTestContentBrowserClient&) = delete;
  WebIdTestContentBrowserClient& operator=(
      const WebIdTestContentBrowserClient&) = delete;

  std::unique_ptr<IdentityRequestDialogController>
  CreateIdentityRequestDialogController(WebContents* web_contents) override;

  std::unique_ptr<DigitalCredentialProvider> CreateDigitalCredentialProvider()
      override;

  // This needs to be called once for every WebID invocation. If there is a
  // need in future to generate these in sequence then a callback can be used.
  void SetIdentityRequestDialogController(
      std::unique_ptr<IdentityRequestDialogController> controller);

  void SetDigitalCredentialProvider(
      std::unique_ptr<DigitalCredentialProvider> provider);

  void SetIdentityRegistry(
      WebContents* web_contents,
      base::WeakPtr<FederatedIdentityModalDialogViewDelegate> delegate,
      const url::Origin& url);

  IdentityRequestDialogController*
  GetIdentityRequestDialogControllerForTests() {
    return test_dialog_controller_.get();
  }

  DigitalCredentialProvider* GetDigitalCredentialProviderForTests() {
    return test_digital_credential_provider_.get();
  }

 private:
  std::unique_ptr<IdentityRequestDialogController> test_dialog_controller_;
  std::unique_ptr<DigitalCredentialProvider> test_digital_credential_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_WEBID_TEST_CONTENT_BROWSER_CLIENT_H_
