// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_
#define DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "device/fido/virtual_fido_device.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) VirtualU2fDevice
    : public VirtualFidoDevice {
 public:
  // Returns true if the |transport| is supported by virtual U2F devices, false
  // otherwise.
  static bool IsTransportSupported(FidoTransportProtocol transport);

  VirtualU2fDevice();
  explicit VirtualU2fDevice(scoped_refptr<State> state);

  VirtualU2fDevice(const VirtualU2fDevice&) = delete;
  VirtualU2fDevice& operator=(const VirtualU2fDevice&) = delete;

  ~VirtualU2fDevice() override;

  // FidoDevice:
  void Cancel(CancelToken) override;
  CancelToken DeviceTransact(std::vector<uint8_t> command,
                             DeviceCallback cb) override;
  base::WeakPtr<FidoDevice> GetWeakPtr() override;

 private:
  std::optional<std::vector<uint8_t>> DoRegister(
      uint8_t ins,
      uint8_t p1,
      uint8_t p2,
      base::span<const uint8_t> data);

  std::optional<std::vector<uint8_t>> DoSign(uint8_t ins,
                                             uint8_t p1,
                                             uint8_t p2,
                                             base::span<const uint8_t> data);

  base::WeakPtrFactory<FidoDevice> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_U2F_DEVICE_H_
