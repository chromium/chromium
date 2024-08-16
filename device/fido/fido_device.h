// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_H_
#define DEVICE_FIDO_FIDO_DEVICE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

namespace cablev2 {
class FidoTunnelDevice;
}

// Device abstraction for an individual CTAP1.0/CTAP2.0 device.
//
// Devices are instantiated with an unknown protocol version. Users should call
// |DiscoverSupportedProtocolAndDeviceInfo| to determine a device's
// capabilities and initialize the instance accordingly. Instances returned by
// |FidoDeviceDiscovery| are not fully initialized.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDevice {
 public:
  // CancelToken is an opaque value that can be used to cancel submitted
  // requests.
  typedef uint32_t CancelToken;
  // kInvalidCancelToken is a |CancelToken| value that will not be returned as
  // the result of |DeviceTransact| and thus can be used as a placeholder.
  static constexpr CancelToken kInvalidCancelToken = 0;

  using DeviceCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>;

  // Internal state machine states.
  enum class State {
    kInit,
    // kConnecting occurs when the device is performing some initialisation. For
    // example, HID devices need to allocate a channel ID before sending
    // requests.
    kConnecting,
    kBusy,
    kReady,
    // kMsgError occurs when the the device responds with an error indicating an
    // invalid command, parameter, or length. This is used within |FidoDevice|
    // to handle the case of a device rejecting a CTAP2 GetInfo command. It is
    // otherwise a fatal, terminal state.
    kMsgError,
    // kDeviceError indicates some error other than those covered by
    // |kMsgError|. This is a terminal state.
    kDeviceError,
  };

  FidoDevice();

  FidoDevice(const FidoDevice&) = delete;
  FidoDevice& operator=(const FidoDevice&) = delete;

  virtual ~FidoDevice();
  // Pure virtual function defined by each device type, implementing
  // the device communication transaction. The function must not immediately
  // call (i.e. hairpin) |callback|.
  virtual CancelToken DeviceTransact(std::vector<uint8_t> command,
                                     DeviceCallback callback) = 0;
  // Attempt to make the device "wink", i.e. grab the attention of the user
  // usually by flashing a light. |callback| is run after a successful wink or
  // if the device does not support winking, in which case it may run
  // immediately.
  virtual void TryWink(base::OnceClosure callback);
  // Cancel attempts to cancel an enqueued request. If the request is currently
  // active it will be aborted if possible, which is expected to cause it to
  // complete with |kCtap2ErrKeepAliveCancel|. If the request is still enqueued
  // it will be deleted and the callback called with
  // |kCtap2ErrKeepAliveCancel| immediately. It is possible that a request to
  // cancel may be unsuccessful and that the request may complete normally.
  // It is safe to attempt to cancel an operation that has already completed.
  virtual void Cancel(CancelToken token) = 0;
  // GetId returns a unique string representing this device. This string should
  // be distinct from all other devices concurrently discovered.
  virtual std::string GetId() const = 0;
  // GetDisplayName returns a string identifying a device to a human, which
  // might not be unique. For example, |GetDisplayName| could return the VID:PID
  // of a HID device, but |GetId| could not because two devices can share the
  // same VID:PID. It defaults to returning the value of |GetId|.
  virtual std::string GetDisplayName() const;
  virtual FidoTransportProtocol DeviceTransport() const = 0;
  virtual cablev2::FidoTunnelDevice* GetTunnelDevice();

  // NoSilentRequests returns true if this device does not support up=false
  // requests.
  bool NoSilentRequests() const;

  virtual base::WeakPtr<FidoDevice> GetWeakPtr() = 0;

  // Sends a speculative AuthenticatorGetInfo request to determine whether the
  // device supports the CTAP2 protocol, and initializes supported_protocol_
  // and device_info_ according to the result.
  virtual void DiscoverSupportedProtocolAndDeviceInfo(base::OnceClosure done);

  // Returns whether supported_protocol has been correctly initialized (usually
  // by calling DiscoverSupportedProtocolAndDeviceInfo).
  bool SupportedProtocolIsInitialized();
  // TODO(martinkr): Rename to "SetSupportedProtocolForTesting".
  void set_supported_protocol(ProtocolVersion supported_protocol) {
    supported_protocol_ = supported_protocol;
  }

  ProtocolVersion supported_protocol() const { return supported_protocol_; }
  const std::optional<AuthenticatorGetInfoResponse>& device_info() const {
    return device_info_;
  }
  bool is_in_error_state() const {
    return state_ == State::kMsgError || state_ == State::kDeviceError;
  }

  // IsStatusForUnrecognisedCredentialID returns true iff the given |status|, in
  // response to a CTAP2 GetAssertion command, indicates that none of the
  // credential IDs was recognised by the authenticator.
  static bool IsStatusForUnrecognisedCredentialID(
      CtapDeviceResponseCode status);

  State state_for_testing() const { return state_; }
  void SetStateForTesting(State state) { state_ = state; }

 protected:
  void OnDeviceInfoReceived(base::OnceClosure done,
                            std::optional<std::vector<uint8_t>> response);
  void SetDeviceInfo(AuthenticatorGetInfoResponse device_info);

  State state_ = State::kInit;
  ProtocolVersion supported_protocol_ = ProtocolVersion::kUnknown;
  std::optional<AuthenticatorGetInfoResponse> device_info_;
  // If `true`, the device needs to be sent a specific wink command to flash
  // when user presence is required.
  bool needs_explicit_wink_ = false;
  // next_cancel_token_ is the value of the next |CancelToken| returned by this
  // device. It starts at one so that zero can be used as an invalid value where
  // needed.
  CancelToken next_cancel_token_ = kInvalidCancelToken + 1;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_H_
