// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_TASK_H_
#define DEVICE_FIDO_FIDO_TASK_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device.h"

namespace device {

// Encapsulates per-device request logic shared between MakeCredential and
// GetAssertion.
//
// TODO(martinkr): FidoTask should be subsumed by FidoDeviceAuthenticator.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoTask {
 public:
  // The |device| must outlive the FidoTask instance.
  explicit FidoTask(FidoDevice* device);

  FidoTask(const FidoTask&) = delete;
  FidoTask& operator=(const FidoTask&) = delete;

  virtual ~FidoTask();

  // Cancel attempts to cancel the operation. This may safely be called at any
  // point but may not be effective because the task may have already completed
  // or the device may not support cancelation. Even if canceled, the callback
  // will still be invoked, albeit perhaps with a status of
  // |kCtap2ErrKeepAliveCancel|.
  virtual void Cancel() = 0;

 protected:
  // Asynchronously initiates CTAP request operation for a single device.
  virtual void StartTask() = 0;

  FidoDevice* device() const {
    DCHECK(device_);
    return device_;
  }

 private:
  const raw_ptr<FidoDevice> device_;
  base::WeakPtrFactory<FidoTask> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_TASK_H_
