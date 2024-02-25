// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_
#define DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

class FidoAuthenticator;

// AuthTokenRequester obtains a pinUvAuthToken from a CTAP2 device.
class COMPONENT_EXPORT(DEVICE_FIDO) AuthTokenRequester {
 public:
  // Result indicates the outcome of running ObtainPINUVAuthToken().  beginning
  // with `kPreTouch` are returned without interaction from the user, which
  // generally means the caller ought to silently ignore this device.
  enum class Result {
    kSuccess,
    kPreTouchUnsatisfiableRequest,
    kPreTouchAuthenticatorResponseInvalid,
    kPostTouchAuthenticatorResponseInvalid,
    kPostTouchAuthenticatorOperationDenied,
    kPostTouchAuthenticatorPINSoftLock,
    kPostTouchAuthenticatorPINHardLock,
    kPostTouchAuthenticatorInternalUVLock,
  };

  // Options configures a AuthTokenRequester.
  struct COMPONENT_EXPORT(DEVICE_FIDO) Options {
    Options();
    Options(Options&&);
    Options& operator=(Options&&);
    ~Options();

    // token_permissions are the pinUvAuthToken permissions to request with the
    // token.
    std::set<pin::Permissions> token_permissions;

    // rp_id is the permissions RP ID for the token to be requested.
    std::optional<std::string> rp_id;

    // skip_pin_touch indicates whether not to request a touch before attempting
    // to obtain a token using a PIN.
    bool skip_pin_touch = false;

    // internal_uv_locked indicates that internal user verification was already
    // locked for the authenticator when building this AuthTokenRequester.
    bool internal_uv_locked = false;
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) Delegate {
   public:
    // ProvidePINCallback is used to provide the AuthTokenRequester with a PIN
    // entered by the user.
    using ProvidePINCallback = base::OnceCallback<void(std::u16string pin)>;

    virtual ~Delegate();

    // AuthenticatorSelectedForPINUVAuthToken is invoked to indicate that the
    // user has interacted with this authenticator (i.e. tapped its button).
    // The Delegate typically uses this signal to cancel outstanding requests to
    // other authenticators. It returns false if another authenticator has
    // already been chosen, and true otherwise. In the former case, no further
    // methods will be called.
    //
    // This method is guaranteed to be called first and at exactly once
    // throughout the handler's lifetime, *unless*
    // HavePINUVAuthTokenResultForAuthenticator() is invoked first with one of
    // the Result codes starting with `kPreTouch`.
    virtual bool AuthenticatorSelectedForPINUVAuthToken(
        FidoAuthenticator* authenticator) = 0;

    // CollectNewPIN is invoked to prompt the user to enter a PIN for an
    // authenticator. |min_pin_length| is the minimum length for a valid PIN.
    // |attempts| is the number of attempts before the authenticator is locked.
    // This number may be zero if the number is unlimited, e.g. when setting a
    // new PIN.
    //
    // The callee must provide the PIN by invoking |provide_pin_cb|. The
    // callback is weakly bound and safe to invoke even after the
    // AuthTokenRequester was freed.
    virtual void CollectPIN(pin::PINEntryReason reason,
                            pin::PINEntryError error,
                            uint32_t min_pin_length,
                            int attempts,
                            ProvidePINCallback provide_pin_cb) = 0;

    // PromptForInternalUVRetry is invoked to prompt the user to retry internal
    // user verification (usually on a fingerprint sensor). |attempts| is the
    // number of remaining attempts before the authenticator is locked. This
    // method may be then be called again if the user verification attempt fails
    // again.
    virtual void PromptForInternalUVRetry(int attempts) = 0;

    // HavePINUVAuthTokenResultForAuthenticator notifies the delegate of the
    // outcome of ObtainPINUVAuthToken(). |response| is `std::nullopt`, unless
    // |result| is |Result::kSuccess|.
    virtual void HavePINUVAuthTokenResultForAuthenticator(
        FidoAuthenticator* authenticator,
        Result result,
        std::optional<pin::TokenResponse> response) = 0;
  };

  // Instantiates a new AuthTokenRequester. |delegate| and |authenticator| must
  // outlive this instance.
  AuthTokenRequester(Delegate* delegate,
                     FidoAuthenticator* authenticator,
                     Options options);
  ~AuthTokenRequester();
  AuthTokenRequester(AuthTokenRequester&) = delete;
  AuthTokenRequester& operator=(AuthTokenRequester&) = delete;
  AuthTokenRequester(AuthTokenRequester&&) = delete;
  AuthTokenRequester& operator=(AuthTokenRequester&&) = delete;

  // ObtainPINUVAuthToken attempts to obtain a pinUvAuthToken from the
  // authenticator.
  void ObtainPINUVAuthToken();

  FidoAuthenticator* authenticator() { return authenticator_; }

 private:
  void ObtainTokenFromInternalUV();
  void OnGetUVRetries(CtapDeviceResponseCode status,
                      std::optional<pin::RetriesResponse> response);
  void OnGetUVToken(CtapDeviceResponseCode status,
                    std::optional<pin::TokenResponse> response);

  void ObtainTokenFromPIN();
  void OnGetPINRetries(CtapDeviceResponseCode status,
                       std::optional<pin::RetriesResponse> response);
  void HavePIN(std::u16string pin);
  void OnGetPINToken(std::string pin,
                     CtapDeviceResponseCode status,
                     std::optional<pin::TokenResponse> response);

  void ObtainTokenFromNewPIN();
  void HaveNewPIN(std::u16string pin);
  void OnSetPIN(std::string pin,
                CtapDeviceResponseCode status,
                std::optional<pin::EmptyResponse> response);

  bool NotifyAuthenticatorSelected();
  void NotifyAuthenticatorSelectedAndFailWithResult(Result result);

  raw_ptr<Delegate> delegate_;
  raw_ptr<FidoAuthenticator> authenticator_;

  Options options_;

  std::optional<bool> authenticator_selected_result_;
  bool is_internal_uv_retry_ = false;
  std::optional<std::string> current_pin_;
  bool internal_uv_locked_ = false;
  bool pin_invalid_ = false;
  int pin_retries_ = 0;

  base::WeakPtrFactory<AuthTokenRequester> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_
