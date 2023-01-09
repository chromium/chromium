// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
#define CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_

#include "base/task/single_thread_task_runner.h"
#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {

class XrDeviceService : public mojom::XRDeviceService {
 public:
  explicit XrDeviceService(
      mojo::PendingReceiver<mojom::XRDeviceService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  XrDeviceService(const XrDeviceService&) = delete;
  XrDeviceService& operator=(const XrDeviceService&) = delete;

  ~XrDeviceService() override;

 private:
  // mojom::XRDeviceService implementation:
  void BindRuntimeProvider(
      mojo::PendingReceiver<mojom::IsolatedXRRuntimeProvider> receiver,
      mojo::PendingRemote<mojom::XRDeviceServiceHost> device_service_host)
      override;
  void BindTestHook(mojo::PendingReceiver<device_test::mojom::XRServiceTestHook>
                        receiver) override;

  mojo::Receiver<mojom::XRDeviceService> receiver_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace device

#endif  // CONTENT_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
