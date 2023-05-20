// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

// This class is responsible for caching and serving CredMan calls. Android U+
// only.
class WebAuthnCredManDelegate : public base::SupportsUserData::Data {
 public:
  explicit WebAuthnCredManDelegate(content::WebContents* web_contents);

  WebAuthnCredManDelegate(const WebAuthnCredManDelegate&) = delete;
  WebAuthnCredManDelegate& operator=(const WebAuthnCredManDelegate&) = delete;

  ~WebAuthnCredManDelegate() override;

  // Called when a Web Authentication Conditional UI request is received. This
  // caches the callback that will complete the request after user
  // interaction.
  void OnCredManConditionalRequestPending(
      content::RenderFrameHost* render_frame_host,
      bool has_results,
      base::RepeatingClosure full_assertion_request);

  // Called when the CredMan UI is closed.
  void OnCredManUiClosed(bool success);

  // Called when the user focuses a webauthn login form. This will trigger
  // CredMan UI.
  void TriggerFullRequest();

  bool HasResults();

  void CleanUpConditionalRequest();

  // The setter for |request_completion_callback_|. Classes can set
  // |request_completion_callback_| to be notified about when CredMan UI is
  // closed (i.e. to show / hide keyboard).
  void SetRequestCompletionCallback(
      base::RepeatingCallback<void(bool)> callback);

  static bool IsCredManEnabled();

  // Returns a delegate associated with the |web_contents|. It creates one if
  // one does not already exist.
  // The delegate is destroyed along with the WebContents and so should not be
  // cached.
  static WebAuthnCredManDelegate* GetRequestDelegate(
      content::WebContents* web_contents);

 private:
  bool has_results_ = false;
  base::RepeatingClosure full_assertion_request_;
  base::RepeatingCallback<void(bool)> request_completion_callback_;
};

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
