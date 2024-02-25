// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_SET_PIN_REQUEST_HANDLER_H_
#define DEVICE_FIDO_SET_PIN_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoAuthenticator;

namespace pin {
struct RetriesResponse;
struct EmptyResponse;
}  // namespace pin

// SetPINRequestHandler handles the Settings UI-based PIN setting flow. It
// flashes all authenticators so that the user can indicate which they want to
// set a PIN on, and then handles (potentially multiple) attempts at setting or
// changing the PIN.
class COMPONENT_EXPORT(DEVICE_FIDO) SetPINRequestHandler
    : public FidoRequestHandlerBase {
 public:
  // GetPINCallback is called once, after the user has touched an authenticator,
  // to request that the user enter a PIN.
  // * The first argument indicates the minimum PIN length for the current PIN,
  //   if set.
  // * The second argument indicates the minimum PIN length for a new PIN.
  // * If the third argument is |nullopt| then the authenticator has no PIN
  //   currently set. Otherwise it indicates the number of attempts remaining.
  using GetPINCallback =
      base::OnceCallback<void(uint32_t current_min_pin_length,
                              uint32_t new_min_pin_length,
                              std::optional<int64_t> attempts)>;

  // FinishedCallback is called multiple times once an attempt has completed.
  // This can be called prior to |GetPINCallback| if the touched authenticator
  // doesn't support setting a PIN. (In which case the error code will be
  // |kCtap1ErrInvalidCommand|.) Otherwise it's called after |ProvidePIN| to
  // report the outcome of an attempt at setting the PIN.
  //
  // Interesting status codes:
  //   |kCtap1ErrInvalidChannel|: authenticator was removed during the process.
  //   |kCtap1ErrInvalidCommand|: touched authenticator does not support PINs.
  //   |kCtap2ErrPinInvalid|: when changing a PIN, the old PIN was incorrect.
  //       In this case only, |ProvidePIN| may be called again to retry.
  using FinishedCallback =
      base::RepeatingCallback<void(CtapDeviceResponseCode)>;

  SetPINRequestHandler(
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      GetPINCallback get_pin_callback,
      FinishedCallback finished_callback,
      std::unique_ptr<FidoDiscoveryFactory> fido_discovery_factory =
          std::make_unique<FidoDiscoveryFactory>());

  SetPINRequestHandler(const SetPINRequestHandler&) = delete;
  SetPINRequestHandler& operator=(const SetPINRequestHandler&) = delete;

  ~SetPINRequestHandler() override;

  // ProvidePIN may be called after |get_pin_callback| has been used to indicate
  // that an attempt at setting the PIN can be made. If the authenticator
  // doesn't currently have a PIN set, then |old_pin| must be the empty string.
  // pin::IsValid(new_pin) must be true when calling.
  void ProvidePIN(const std::string& old_pin, const std::string& new_pin);

 private:
  enum class State {
    kWaitingForTouch,
    kGettingRetries,
    kWaitingForPIN,
    kSettingPIN,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void RequestRetries();
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         std::optional<pin::RetriesResponse> response);

  void OnSetPINComplete(CtapDeviceResponseCode status,
                        std::optional<pin::EmptyResponse> response);

  State state_ = State::kWaitingForTouch;
  GetPINCallback get_pin_callback_;
  FinishedCallback finished_callback_;
  // authenticator_ is the authenticator that was selected by the initial touch.
  // The pointed-at object is owned by the |FidoRequestHandlerBase| superclass
  // of this class.
  raw_ptr<FidoAuthenticator> authenticator_ = nullptr;
  std::unique_ptr<FidoDiscoveryFactory> fido_discovery_factory_;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<SetPINRequestHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_SET_PIN_REQUEST_HANDLER_H_
