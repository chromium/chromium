// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_DEVICE_AUTHENTICATOR_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_DEVICE_AUTHENTICATOR_H_

#import <string>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "components/device_reauth/device_authenticator.h"

@protocol CWVAutofillClientIOSBridge;

namespace autofill {

// ChromeWebView implementation of `device_reauth::DeviceAuthenticator`.
//
// Device capability checks are answered directly via `LAContext`. The actual
// authentication prompt is delegated to the embedder through `bridge`, since
// ChromeWebView has no reauthentication UI of its own.
class WebViewDeviceAuthenticator : public device_reauth::DeviceAuthenticator {
 public:
  // `bridge` is held weakly. If it is deallocated before
  // `AuthenticateWithMessage()` is called, authentication fails immediately.
  explicit WebViewDeviceAuthenticator(id<CWVAutofillClientIOSBridge> bridge);

  WebViewDeviceAuthenticator(const WebViewDeviceAuthenticator&) = delete;
  WebViewDeviceAuthenticator& operator=(const WebViewDeviceAuthenticator&) =
      delete;

  ~WebViewDeviceAuthenticator() override;

  // device_reauth::DeviceAuthenticator:
  bool CanAuthenticateWithBiometrics() override;
  bool CanAuthenticateWithBiometricOrScreenLock() override;
  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;
  void Cancel() override;

 private:
  // Runs `callback_` with `success`.
  void OnAuthenticationCompleted(bool success);

  __weak id<CWVAutofillClientIOSBridge> bridge_;

  // Callback for the in-flight authentication, if any.
  AuthenticateCallback callback_;

  base::WeakPtrFactory<WebViewDeviceAuthenticator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_DEVICE_AUTHENTICATOR_H_
