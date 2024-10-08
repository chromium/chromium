// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_TRANSACTION_IMPL_H_
#define CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_TRANSACTION_IMPL_H_

#include <array>
#include <cstdint>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "content/browser/webid/digital_credentials/cross_device_request_dispatcher.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace content::digital_credentials::cross_device {

// Performs a cross-device digital identity transaction by listening for mobile
// devices that have scanned a QR code and thus are broadcasting a BLE message.
// An encrypted tunnel is set up between this device and the mobile device and
// the digital identity is exchanged.
class CONTENT_EXPORT TransactionImpl : public Transaction,
                                       device::BluetoothAdapter::Observer {
 public:
  TransactionImpl(
      // The origin of the requesting page.
      url::Origin origin,
      // The request, as would be found in place of "$1" in the following
      // Javascript:  `navigator.identity.get({digital: $1});`
      base::Value request,
      // A secret key that was used to generate the QR code. Any mobile devices
      // will have to prove that they know this secret because they scanned the
      // QR code.
      std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key,
      device::NetworkContextFactory network_context_factory,
      // This callback may be called multiple times as the process advances.
      Transaction::EventCallback event_callback,
      // This callback will be called exactly once. After which, no more events
      // will be reported.
      Transaction::CompletionCallback callback);
  TransactionImpl(const TransactionImpl&) = delete;
  TransactionImpl& operator=(const TransactionImpl&) = delete;
  TransactionImpl(TransactionImpl&& other) = delete;
  TransactionImpl& operator=(TransactionImpl&& other) = delete;
  ~TransactionImpl() override;

  // Turn on the BLE adapter. Only valid to call if
  // `Event::kBluetoothNotPowered` was emitted.
  void PowerBluetoothAdapter() override;

  // device::BluetoothAdapter::Observer
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

 private:
  void OnCableEvent(device::cablev2::Event);
  void OnHaveAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void ConsiderPowerState();
  void OnHaveBluetoothPermission(device::BluetoothAdapter::PermissionStatus);
  void MaybeSignalReady();
  void OnHaveResponse(base::expected<Response, RequestDispatcher::Error>);

  const url::Origin origin_;
  base::Value request_;
  const Transaction::EventCallback event_callback_;
  Transaction::CompletionCallback callback_;

  std::unique_ptr<RequestDispatcher> dispatcher_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  bool waiting_for_power_ = false;
  bool waiting_for_permission_ = false;
  bool running_signaled_ = false;

  base::WeakPtrFactory<TransactionImpl> weak_ptr_factory_{this};
};

}  // namespace content::digital_credentials::cross_device

#endif  // CONTENT_BROWSER_WEBID_DIGITAL_CREDENTIALS_CROSS_DEVICE_TRANSACTION_IMPL_H_
