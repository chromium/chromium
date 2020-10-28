// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_
#define DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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
    base::Optional<std::string> rp_id;

    // skip_pin_touch indicates whether not to request a touch before attempting
    // to obtain a token using a PIN.
    bool skip_pin_touch = false;
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) Delegate {
   public:
    // ProvidePINCallback is used to provide the AuthTokenRequester with a PIN
    // entered by the user. Callers must use |pin::IsValid()| to validate |pin|
    // before invoking this callback.
    using ProvidePINCallback = base::OnceCallback<void(std::string pin)>;

    virtual ~Delegate();

    // AuthenticatorSelectedForPINUVAuthToken is invoked to indicate that the
    // user has interacted with this authenticator (i.e. tapped its button).
    // The Delegate typically uses this signal to cancel outstanding requests to
    // other authenticators.
    //
    // This method is guaranteed to be called first and at exactly once
    // throughout the handler's lifetime, *unless*
    // HavePINUVAuthTokenResultForAuthenticator() is invoked first with one of
    // the Result codes starting with `kPreTouch`.
    virtual void AuthenticatorSelectedForPINUVAuthToken(
        FidoAuthenticator* authenticator) = 0;

    // CollectNewPIN is invoked to prompt the user to enter a new PIN for an
    // authenticator.
    //
    // The callee must provide the PIN by invoking |provide_pin_cb|. The
    // callback is weakly bound and safe to invoke even after the
    // AuthTokenRequester was freed.
    virtual void CollectNewPIN(ProvidePINCallback provide_pin_cb) = 0;

    // CollectExistingPIN is invoked to prompt the user to provide the existing
    // PIN to an authenticator. |attempts| is the number of remaining attempts
    // before the authenticator is locked.
    //
    // The callee must provide the PIN by invoking |provide_pin_cb|. The
    // callback is weakly bound and safe to invoke even after the
    // AuthTokenRequester was freed. If CollectExistingPIN() is called again
    // after callback invocation, the provided PIN was incorrect.
    virtual void CollectExistingPIN(int attempts,
                                    ProvidePINCallback provide_pin_cb) = 0;

    // InternalUVLockedForAuthToken() notifies the delegate that the
    // authenticator's internal modality was locked due to too many invalid
    // authentication attempts. It is followed by a call to CollectExistingPIN()
    // to prompt for a PIN as a fallback authentication mechanism.
    virtual void InternalUVLockedForAuthToken() = 0;

    // PromptForInternalUVRetry is invoked to prompt the user to retry internal
    // user verification (usually on a fingerprint sensor). |attempts| is the
    // number of remaining attempts before the authenticator is locked. This
    // method may be then be called again if the user verification attempt fails
    // again.
    virtual void PromptForInternalUVRetry(int attempts) = 0;

    // HavePINUVAuthTokenResultForAuthenticator notifies the delegate of the
    // outcome of ObtainPINUVAuthToken(). |response| is `base::nullopt`, unless
    // |result| is |Result::kSuccess|.
    virtual void HavePINUVAuthTokenResultForAuthenticator(
        FidoAuthenticator* authenticator,
        Result result,
        base::Optional<pin::TokenResponse> response) = 0;
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
                      base::Optional<pin::RetriesResponse> response);
  void OnGetUVToken(CtapDeviceResponseCode status,
                    base::Optional<pin::TokenResponse> response);

  void ObtainTokenFromPIN();
  void OnGetPINRetries(CtapDeviceResponseCode status,
                       base::Optional<pin::RetriesResponse> response);
  void HavePIN(std::string pin);
  void OnGetPINToken(CtapDeviceResponseCode status,
                     base::Optional<pin::TokenResponse> response);

  void ObtainTokenFromNewPIN();
  void HaveNewPIN(std::string pin);
  void OnSetPIN(std::string pin,
                CtapDeviceResponseCode status,
                base::Optional<pin::EmptyResponse> response);

  void NotifyAuthenticatorSelected();
  void NotifyAuthenticatorSelectedAndFailWithResult(Result result);

  Delegate* delegate_;
  FidoAuthenticator* authenticator_;

  Options options_;

  bool authenticator_was_selected_ = false;
  bool is_internal_uv_retry_ = false;

  base::WeakPtrFactory<AuthTokenRequester> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTH_TOKEN_REQUESTER_H_
