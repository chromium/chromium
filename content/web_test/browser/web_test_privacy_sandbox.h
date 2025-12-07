// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRIVACY_SANDBOX_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRIVACY_SANDBOX_H_

#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/test/mojom/privacy_sandbox/web_privacy_sandbox_automation.test-mojom.h"

namespace content {

class WebContents;

// Implementation of the WebPrivacySandboxAutomation Mojo interface for use by
// Blink's InternalsProtectedAudience, used for content_shell impl of
// testdriver.js
class WebTestPrivacySandbox
    : public blink::test::mojom::WebPrivacySandboxAutomation,
      public WebContentsUserData<WebTestPrivacySandbox> {
 public:
  static WebTestPrivacySandbox* GetOrCreate(WebContents*);

  WebTestPrivacySandbox(const WebTestPrivacySandbox&) = delete;
  WebTestPrivacySandbox& operator=(const WebTestPrivacySandbox&) = delete;

  ~WebTestPrivacySandbox() override;

  void Bind(
      mojo::PendingReceiver<blink::test::mojom::WebPrivacySandboxAutomation>
          receiver);

  // blink::mojom::WebPrivacySandboxAutomation overrides.
  void SetProtectedAudienceKAnonymity(
      const url::Origin& owner_origin,
      const std::string& name,
      const std::vector<std::string>& hashes,
      SetProtectedAudienceKAnonymityCallback callback) override;

 private:
  explicit WebTestPrivacySandbox(WebContents* web_contents);
  mojo::ReceiverSet<blink::test::mojom::WebPrivacySandboxAutomation> receivers_;

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_PRIVACY_SANDBOX_H_
