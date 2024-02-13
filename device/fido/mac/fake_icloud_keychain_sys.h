// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_SYS_H_
#define DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_SYS_H_

#if !defined(__OBJC__)
#error "This header is only for Objective C++ compilation units"
#endif

#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/mac/icloud_keychain_sys.h"

@class NSWindow;

namespace device::fido::icloud_keychain {

// FakeSystemInterface allows the macOS passkey API to be simulated for tests.
// One can create an instance with `base::MakeRefCounted<FakeSystemInterface>()`
// and install it with `SetSystemInterfaceForTesting`.
class API_AVAILABLE(macos(13.3)) FakeSystemInterface : public SystemInterface {
 public:
  FakeSystemInterface();

  // set_auth_state sets the state that `GetAuthState` will report.
  void set_auth_state(AuthState auth_state);

  // set_next_auth_state sets the state the state that will become current after
  // a call to `AuthorizeAndContinue`. This must be called before
  // `AuthorizeAndContinue`.
  void set_next_auth_state(AuthState next_auth_state);

  // cancel_count returns the number of times that `Cancel` has been called.
  unsigned cancel_count() const { return cancel_count_; }

  // SetMakeCredentialResult sets the values that will be returned from the next
  // call to `MakeCredential`. If not set, `MakeCredential` will return an
  // error.
  void SetMakeCredentialResult(base::span<const uint8_t> attestation_obj_bytes,
                               base::span<const uint8_t> credential_id);

  // SetMakeCredentialError sets the code of the `NSError` that will be returned
  // from the next `MakeCredential` call.
  void SetMakeCredentialError(int code);

  // SetGetAssertionResult sets the values that will be returned from the next
  // call to `GetAssertion`. If not set, `GetAssertion` will return an error.
  void SetGetAssertionResult(base::span<const uint8_t> authenticator_data,
                             base::span<const uint8_t> signature,
                             base::span<const uint8_t> user_id,
                             base::span<const uint8_t> credential_id);

  // SetMakeCredentialError configures the `NSError` that will be returned
  // from the next `GetAssertion` call.
  void SetGetAssertionError(int code, std::string msg);

  // SetCredentials causes `GetPlatformCredentials` to simulate that the given
  // credentials are on the system. (Note that `GetPlatformCredentials` ignores
  // the requested RP ID so all credentials specified here will be returned.)
  void SetCredentials(std::vector<DiscoverableCredentialMetadata> creds);

  // SystemInterface:
  bool IsAvailable() const override;

  AuthState GetAuthState() override;

  void AuthorizeAndContinue(base::OnceCallback<void()> callback) override;

  void GetPlatformCredentials(
      const std::string& rp_id,
      void (^)(NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*))
      override;

  void MakeCredential(
      NSWindow* window,
      CtapMakeCredentialRequest request,
      base::OnceCallback<void(ASAuthorization*, NSError*)> callback) override;

  void GetAssertion(
      NSWindow* window,
      CtapGetAssertionRequest request,
      base::OnceCallback<void(ASAuthorization*, NSError*)> callback) override;

  void Cancel() override;

 protected:
  friend class base::RefCounted<SystemInterface>;
  friend class base::RefCounted<FakeSystemInterface>;
  ~FakeSystemInterface() override;

  AuthState auth_state_ = kAuthAuthorized;
  std::optional<AuthState> next_auth_state_;

  std::optional<int> make_credential_error_code_;
  std::optional<std::vector<uint8_t>> make_credential_attestation_object_bytes_;
  std::optional<std::vector<uint8_t>> make_credential_credential_id_;

  std::optional<std::pair<int, std::string>> get_assertion_error_;
  std::optional<std::vector<uint8_t>> get_assertion_authenticator_data_;
  std::optional<std::vector<uint8_t>> get_assertion_signature_;
  std::optional<std::vector<uint8_t>> get_assertion_user_id_;
  std::optional<std::vector<uint8_t>> get_assertion_credential_id_;

  unsigned cancel_count_ = 0;

  std::vector<DiscoverableCredentialMetadata> creds_;
};

}  // namespace device::fido::icloud_keychain

#endif  // DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_SYS_H_
