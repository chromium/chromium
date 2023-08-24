// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_SYS_H_
#define DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_SYS_H_

#include <memory>

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"

@class NSWindow;

namespace device::fido::icloud_keychain {

// SystemInterface is the lowest-level abstraction for iCloud Keychain support.
// Automated tests can't reach below this point without triggering dialogs on
// the machine.
class COMPONENT_EXPORT(DEVICE_FIDO) API_AVAILABLE(macos(13.3)) SystemInterface
    : public base::RefCounted<SystemInterface> {
 public:
  // IsAvailable returns true if the other functions in this interface can be
  // called.
  virtual bool IsAvailable() const = 0;

  // These names are extremely long and so are aliased here to make other code
  // a bit more readable.
  using AuthState =
      ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState;
  static constexpr auto kAuthNotAuthorized =
      ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateNotDetermined;
  static constexpr auto kAuthDenied =
      ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateDenied;
  static constexpr auto kAuthAuthorized =
      ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateAuthorized;

  // GetAuthState returns the current level of authorization for calling
  // `GetPlatformCredentials`.
  virtual AuthState GetAuthState() = 0;

  // AuthorizeAndContinue requests authorization for calling
  // `GetPlatformCredentials` and, successful or not, then calls `callback`.
  virtual void AuthorizeAndContinue(base::OnceCallback<void()> callback) = 0;

  // GetPlatformCredentials enumerates the credentials for `rp_id`.
  virtual void GetPlatformCredentials(
      const std::string& rp_id,
      void (^)(
          NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*)) = 0;

  virtual void MakeCredential(
      NSWindow* window,
      CtapMakeCredentialRequest request,
      base::OnceCallback<void(ASAuthorization*, NSError*)> callback) = 0;

  virtual void GetAssertion(
      NSWindow* window,
      CtapGetAssertionRequest request,
      base::OnceCallback<void(ASAuthorization*, NSError*)> callback) = 0;

  virtual void Cancel() = 0;

 protected:
  friend class base::RefCounted<SystemInterface>;
  virtual ~SystemInterface();
};

// GetSystemInterface returns the current implementation of `SystemInterface`.
COMPONENT_EXPORT(DEVICE_FIDO)
API_AVAILABLE(macos(13.3)) scoped_refptr<SystemInterface> GetSystemInterface();

COMPONENT_EXPORT(DEVICE_FIDO)
API_AVAILABLE(macos(13.3))
void SetSystemInterfaceForTesting(scoped_refptr<SystemInterface> sys_interface);

}  // namespace device::fido::icloud_keychain

#endif  // DEVICE_FIDO_MAC_ICLOUD_KEYCHAIN_SYS_H_
