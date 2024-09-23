// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_H_
#define CONTENT_PUBLIC_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_H_

#include <vector>

#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/network_context_factory.h"
#include "url/origin.h"

namespace content::digital_credentials::cross_device {

// SystemErrors result from issues with the local computer that prevent a
// transaction from completing.
enum class SystemError {
  // This is returned if a macOS process hasn't been launched as "self
  // responsible". Without this, the Bluetooth permission is based on the
  // launching application, often a terminal. If the terminal didn't happen to
  // confingure itself for Bluetooth permission then the Chrome process is
  // killed by the kernel.
  kNotSelfResponsible,
  // There's no Bluetooth adaptor, or it doesn't support BLE.
  kNoBleSupport,
  // Returned if BLE permission has been denied to the point where it cannot
  // be requested. The user will need to update their system settings.
  kPermissionDenied,
  // BLE was powered off during the transaction.
  kLostPower,
};

// ProtocolErrors result from issues communicating with a remote device where
// the remote device is at fault.
enum class ProtocolError {
  kIncompatibleDevice,
  kTransportError,
  kInvalidResponse,
};

// RemoteErrors are reported by the remote device. These values are taken from
// the CTAP 2.2 spec.
enum class RemoteError {
  kUserCanceled,
  kDeviceAborted,
  kNoCredential,
  kOther,  // All unknown error values are mapped to this.
};

enum class SystemEvent {
  // The BLE adapter is off. `Transaction::PowerBluetoothAdapter` can be called
  // to turn it on, but the user should have indicated that they wish to first.
  kBluetoothNotPowered,
  // The user has been prompted for Bluetooth permission. The system will show
  // a dialog to them for them to approve. No action is needed by the caller:
  // either the user will grant permission and the `kReady` event will be
  // reported, or the user will deny permission and the transaction will fail
  // with `SystemError::kPermissionDenied`.
  kNeedPermission,
  // The system is listening for BLE adverts.
  kReady,
};

using Error = absl::variant<SystemError, ProtocolError, RemoteError>;

// Events either come from the underlying hybrid connection, or are
// SystemEvents.
using Event = absl::variant<device::cablev2::Event, SystemEvent>;

// A Response is the response to a cross-device request. At this level of
// abstraction it's an opaque `base::Value` taken from the JSON reply.
using Response = base::StrongAlias<class CrossDeviceResponseTag, base::Value>;

// A Transaction performs a cross-device digital identity transaction by
// listening for mobile devices that have scanned a QR code and thus are
// broadcasting a BLE message. An encrypted tunnel is set up between this device
// and the mobile device and the digital identity is exchanged.
class CONTENT_EXPORT Transaction {
 public:
  using EventCallback = base::RepeatingCallback<void(Event)>;
  using CompletionCallback =
      base::OnceCallback<void(base::expected<Response, Error>)>;

  static std::unique_ptr<Transaction> New(
      // The origin of the requesting page.
      url::Origin origin,
      // The request, as would be found in place of "$1" in the following
      // Javascript: `navigator.identity.get({digital: $1});`
      base::Value request,
      // A secret key that was used to generate the generated QR code. Any
      // mobile devices will have to prove that they know this secret because
      // they scanned the QR code.
      std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key,
      device::NetworkContextFactory network_context_factory,
      // This callback may be called multiple times as the process advances.
      EventCallback event_callback,
      // This callback will be called exactly once. After which, no more events
      // will be reported.
      CompletionCallback callback);

  virtual ~Transaction();

  // Turn on the Bluetooth adapter. Should only be called if
  // `kBluetoothNotPowered` is reported and the user wishes to.
  virtual void PowerBluetoothAdapter() = 0;
};

}  // namespace content::digital_credentials::cross_device

#endif  // CONTENT_PUBLIC_BROWSER_DIGITAL_CREDENTIALS_CROSS_DEVICE_H_
