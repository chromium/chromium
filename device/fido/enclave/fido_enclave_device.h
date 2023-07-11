// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_FIDO_ENCLAVE_DEVICE_H_
#define DEVICE_FIDO_ENCLAVE_FIDO_ENCLAVE_DEVICE_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_device.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace device {

class EnclaveHttpClient;

namespace cablev2 {
class Crypter;
class HandshakeInitiator;
}  // namespace cablev2

class FidoEnclaveDevice : public FidoDevice {
 public:
  FidoEnclaveDevice(
      const GURL& service_url,
      base::span<const uint8_t, device::kP256X962Length> peer_identity);
  ~FidoEnclaveDevice() override;

  FidoEnclaveDevice(const FidoEnclaveDevice&) = delete;
  FidoEnclaveDevice& operator=(const FidoEnclaveDevice&) = delete;

  // FidoDevice:
  FidoDevice::CancelToken DeviceTransact(std::vector<uint8_t> command,
                                         DeviceCallback callback) override;
  std::string GetId() const override;
  void Cancel(FidoDevice::CancelToken token) override {}
  FidoTransportProtocol DeviceTransport() const override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  enum class State {
    kInitialized,
    kWaitingForHandshakeResponse,
    kConnected,
    kError,
  };

  void OnResponseReceived(int status,
                          absl::optional<std::vector<uint8_t>> data);
  void SendCtapCommand();

  State state_ = State::kInitialized;

  std::unique_ptr<EnclaveHttpClient> http_client_;

  // The peer's public key.
  const std::array<uint8_t, device::kP256X962Length> peer_identity_;

  std::unique_ptr<cablev2::HandshakeInitiator> handshake_;
  absl::optional<std::array<uint8_t, 32>> handshake_hash_;
  std::unique_ptr<cablev2::Crypter> crypter_;

  // DeviceTransact arguments while waiting the connection to be established.
  std::vector<uint8_t> pending_message_;
  DeviceCallback pending_callback_;

  base::WeakPtrFactory<FidoEnclaveDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_FIDO_ENCLAVE_DEVICE_H_
