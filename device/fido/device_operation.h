// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DEVICE_OPERATION_H_
#define DEVICE_FIDO_DEVICE_OPERATION_H_

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

// GenericDeviceOperation is a base class to allow a |DeviceOperation| to be
// held in |std::unique_ptr| without having to know the concrete type of the
// operation.
class GenericDeviceOperation {
 public:
  virtual ~GenericDeviceOperation() = default;
  virtual void Start() = 0;

  // Cancel will attempt to cancel the current operation. It is safe to call
  // this function both before |Start| and after the operation has completed.
  virtual void Cancel() = 0;
};

template <class Request, class Response>
class DeviceOperation : public GenericDeviceOperation {
 public:
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode, std::optional<Response>)>;
  // Represents a per device logic that is owned by FidoTask. Thus,
  // DeviceOperation does not outlive |request|.
  DeviceOperation(FidoDevice* device,
                  Request request,
                  DeviceResponseCallback callback)
      : device_(device),
        request_(std::move(request)),
        callback_(std::move(callback)) {}

  DeviceOperation(const DeviceOperation&) = delete;
  DeviceOperation& operator=(const DeviceOperation&) = delete;

  ~DeviceOperation() override = default;

 protected:
  void DispatchU2FCommand(std::optional<std::vector<uint8_t>> command,
                          FidoDevice::DeviceCallback callback) {
    if (!command || device_->is_in_error_state()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    token_ = device_->DeviceTransact(std::move(*command), std::move(callback));
  }

  const Request& request() const { return request_; }
  FidoDevice* device() const { return device_; }
  DeviceResponseCallback callback() { return std::move(callback_); }
  std::optional<FidoDevice::CancelToken> token_;

 private:
  const raw_ptr<FidoDevice> device_ = nullptr;
  Request request_;
  DeviceResponseCallback callback_;
};

}  // namespace device

#endif  // DEVICE_FIDO_DEVICE_OPERATION_H_
