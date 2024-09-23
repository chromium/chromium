// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/types/strong_alias.h"

namespace content {
class WebContents;
}  // namespace content

namespace webauthn {

// This class is responsible for caching and serving CredMan calls. Android U+
// only.
class WebAuthnCredManDelegate {
 public:
  using RequestPasswords = base::StrongAlias<class RequestPasswordsTag, bool>;

  enum State {
    kNotReady,
    kNoPasskeys,
    kHasPasskeys,
  };

  enum CredManEnabledMode {
    kNotEnabled,
    kAllCredMan,
    kNonGpmPasskeys,
  };

  explicit WebAuthnCredManDelegate(content::WebContents* web_contents);

  WebAuthnCredManDelegate(const WebAuthnCredManDelegate&) = delete;
  WebAuthnCredManDelegate& operator=(const WebAuthnCredManDelegate&) = delete;

  virtual ~WebAuthnCredManDelegate();

  // Called when a Web Authentication Conditional UI request is received. This
  // caches the callback that will complete the request after user
  // interaction.
  virtual void OnCredManConditionalRequestPending(
      bool has_results,
      base::RepeatingCallback<void(bool)> show_cred_man_ui_callback);

  // Called when the CredMan UI is closed.
  virtual void OnCredManUiClosed(bool success);

  // Called when the user focuses a webauthn login form. This will trigger
  // CredMan UI.
  // If |request_passwords|, the UI will also include passwords if there are
  // any.
  virtual void TriggerCredManUi(RequestPasswords request_passwords);

  // Returns whether there are passkeys in the Android Credential Manager UI.
  // Returns `kNotReady` if Credential Manager has not replied yet.
  virtual State HasPasskeys() const;

  // Clears the cached `show_cred_man_ui_callback_` and `has_results_`.
  virtual void CleanUpConditionalRequest();

  // The setter for `request_completion_callback_`. Classes can set
  // `request_completion_callback_` to be notified about when CredMan UI is
  // closed (i.e. to show / hide keyboard).
  virtual void SetRequestCompletionCallback(
      base::RepeatingCallback<void(bool)> callback);

  // The setter for `filling_callback_`.  Classes should use this method before
  // `FillUsernameAndPassword`.
  virtual void SetFillingCallback(
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          filling_callback);

  // If a password credential is received from CredMan UI, this method will be
  // called. A password credential can be filled only once.
  virtual void FillUsernameAndPassword(const std::u16string& username,
                                       const std::u16string& password);

  static CredManEnabledMode CredManMode();

#if defined(UNIT_TEST)
  static void override_cred_man_support_for_testing(int support) {
    cred_man_support_ = support;
  }
#endif

 private:
  State has_passkeys_ = kNotReady;
  base::RepeatingCallback<void(bool)> show_cred_man_ui_callback_;
  base::RepeatingCallback<void(bool)> request_completion_callback_;
  base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      filling_callback_;

  static std::optional<int> cred_man_support_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
