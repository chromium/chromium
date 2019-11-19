// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DEVICE_OPERATION_H_
#define DEVICE_FIDO_DEVICE_OPERATION_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

// GenericDeviceOperation is a base class to allow a |DeviceOperation| to be
// held in |std::unique_ptr| without having to know the concrete type of the
// operation.
class GenericDeviceOperation {
 public:
  virtual ~GenericDeviceOperation() {}
  virtual void Start() = 0;

  // Cancel will attempt to cancel the current operation. It is safe to call
  // this function both before |Start| and after the operation has completed.
  virtual void Cancel() = 0;
};

template <class Request, class Response>
class DeviceOperation : public GenericDeviceOperation {
 public:
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<Response>)>;
  // Represents a per device logic that is owned by FidoTask. Thus,
  // DeviceOperation does not outlive |request|.
  DeviceOperation(FidoDevice* device,
                  Request request,
                  DeviceResponseCallback callback)
      : device_(device),
        request_(std::move(request)),
        callback_(std::move(callback)) {}

  virtual ~DeviceOperation() = default;

 protected:
  // TODO(hongjunchoi): Refactor so that |command| is never base::nullopt.
  void DispatchDeviceRequest(base::Optional<std::vector<uint8_t>> command,
                             FidoDevice::DeviceCallback callback) {
    if (!command || device_->is_in_error_state()) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    token_ = device_->DeviceTransact(std::move(*command), std::move(callback));
  }

  const Request& request() const { return request_; }
  FidoDevice* device() const { return device_; }
  DeviceResponseCallback callback() { return std::move(callback_); }
  base::Optional<FidoDevice::CancelToken> token_;

 private:
  FidoDevice* const device_ = nullptr;
  Request request_;
  DeviceResponseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_DEVICE_OPERATION_H_
