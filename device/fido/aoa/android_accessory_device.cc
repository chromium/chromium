// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/aoa/android_accessory_device.h"

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

static constexpr unsigned kTimeoutMilliseconds = 1000;
static constexpr unsigned kLongTimeoutMilliseconds = 90 * 1000;

AndroidAccessoryDevice::AndroidAccessoryDevice(
    mojo::Remote<mojom::UsbDevice> device,
    uint8_t in_endpoint,
    uint8_t out_endpoint)
    : device_(std::move(device)),
      in_endpoint_(in_endpoint),
      out_endpoint_(out_endpoint) {
  base::RandBytes(id_, sizeof(id_));
}

AndroidAccessoryDevice::~AndroidAccessoryDevice() = default;

FidoDevice::CancelToken AndroidAccessoryDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback callback) {
  if (static_cast<uint64_t>(command.size()) >
      std::numeric_limits<uint32_t>::max()) {
    NOTREACHED();
    std::move(callback).Run(absl::nullopt);
    return 0;
  }

  uint8_t prefix[1 + sizeof(uint32_t)];
  prefix[0] = kCoaoaMsg;
  const uint32_t size32 = static_cast<uint32_t>(command.size());
  memcpy(&prefix[1], &size32, sizeof(size32));

  command.insert(command.begin(), prefix, &prefix[sizeof(prefix)]);

  device_->GenericTransferOut(
      out_endpoint_, std::move(command), kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDevice::OnWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  return 0;
}

void AndroidAccessoryDevice::OnWriteComplete(DeviceCallback callback,
                                             mojom::UsbTransferStatus result) {
  if (result != mojom::UsbTransferStatus::COMPLETED) {
    FIDO_LOG(ERROR) << "Failed to write to USB device ("
                    << static_cast<int>(result) << ").";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  device_->GenericTransferIn(
      in_endpoint_, 1 + sizeof(uint32_t), kLongTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDevice::OnReadLengthComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AndroidAccessoryDevice::OnReadLengthComplete(
    DeviceCallback callback,
    mojom::UsbTransferStatus result,
    base::span<const uint8_t> payload) {
  if (result != mojom::UsbTransferStatus::COMPLETED ||
      payload.size() != 1 + sizeof(uint32_t)) {
    FIDO_LOG(ERROR) << "Failed to read reply from USB device ("
                    << static_cast<int>(result) << ")";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (payload[0] != kCoaoaMsg) {
    FIDO_LOG(ERROR) << "Reply from USB device with wrong type ("
                    << static_cast<int>(payload[0]) << ")";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  uint32_t length;
  memcpy(&length, &payload[1], sizeof(length));
  if (length > (1 << 20)) {
    FIDO_LOG(ERROR) << "USB device sent excessive reply containing " << length
                    << " bytes";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  buffer_.clear();
  buffer_.reserve(length);

  if (length == 0) {
    std::move(callback).Run(std::move(buffer_));
    return;
  }

  device_->GenericTransferIn(
      in_endpoint_, length, kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDevice::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), length));
}

void AndroidAccessoryDevice::OnReadComplete(DeviceCallback callback,
                                            const uint32_t length,
                                            mojom::UsbTransferStatus result,
                                            base::span<const uint8_t> payload) {
  if (result != mojom::UsbTransferStatus::COMPLETED ||
      payload.size() + buffer_.size() > length) {
    FIDO_LOG(ERROR) << "Failed to read from USB device ("
                    << static_cast<int>(result) << ")";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  buffer_.insert(buffer_.end(), payload.begin(), payload.end());
  if (buffer_.size() == length) {
    std::move(callback).Run(std::move(buffer_));
    return;
  }

  device_->GenericTransferIn(
      in_endpoint_, length - buffer_.size(), kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDevice::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), length));
}

void AndroidAccessoryDevice::Cancel(CancelToken token) {}

std::string AndroidAccessoryDevice::GetId() const {
  return "aoa-" + base::HexEncode(id_);
}

FidoTransportProtocol AndroidAccessoryDevice::DeviceTransport() const {
  return FidoTransportProtocol::kAndroidAccessory;
}

base::WeakPtr<FidoDevice> AndroidAccessoryDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace device
