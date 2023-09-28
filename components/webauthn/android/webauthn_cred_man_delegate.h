// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
#define COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_

#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace webauthn {

// This class is responsible for caching and serving CredMan calls. Android U+
// only.
class WebAuthnCredManDelegate {
 public:
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

  ~WebAuthnCredManDelegate();

  // Called when a Web Authentication Conditional UI request is received. This
  // caches the callback that will complete the request after user
  // interaction.
  void OnCredManConditionalRequestPending(
      bool has_results,
      base::RepeatingCallback<void(bool)> show_cred_man_ui_callback);

  // Called when the CredMan UI is closed.
  void OnCredManUiClosed(bool success);

  // Called when the user focuses a webauthn login form. This will trigger
  // CredMan UI.
  void TriggerCredManUi();

  // Returns whether there are passkeys in the Android Credential Manager UI.
  // Returns `kNotReady` if Credential Manager has not replied yet.
  State HasPasskeys();

  // Clears the cached `show_cred_man_ui_callback_` and `has_results_`.
  void CleanUpConditionalRequest();

  // The setter for `request_completion_callback_`. Classes can set
  // `request_completion_callback_` to be notified about when CredMan UI is
  // closed (i.e. to show / hide keyboard).
  void SetRequestCompletionCallback(
      base::RepeatingCallback<void(bool)> callback);

  // The setter for `filling_callback_`.  Classes should use this method before
  // `FillUsernameAndPassword`.
  void SetFillingCallback(
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
          filling_callback);

  // If a password credential is received from CredMan UI, this method will be
  // called. A password credential can be filled only once.
  void FillUsernameAndPassword(const std::u16string& username,
                               const std::u16string& password);

  static CredManEnabledMode CredManMode();

#if defined(UNIT_TEST)
  static void override_android_version_for_testing(bool should_override) {
    override_android_version_for_testing_ = should_override;
  }
#endif

 private:
  State has_passkeys_ = kNotReady;
  base::RepeatingCallback<void(bool)> show_cred_man_ui_callback_;
  base::RepeatingCallback<void(bool)> request_completion_callback_;
  base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      filling_callback_;

  // This bool is required to override android version check in
  // `IsCredManEnabled` because UNIT_TEST cannot be evaluated in the cc file for
  // tests. It should be `false` for non-tests!
  static bool override_android_version_for_testing_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_ANDROID_WEBAUTHN_CRED_MAN_DELEGATE_H_
