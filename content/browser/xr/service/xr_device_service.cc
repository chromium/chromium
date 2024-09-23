// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_device_service.h"

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/gpu_client.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

base::RepeatingClosure& GetStartupCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

// XRDeviceServiceHostImpl is the browser process implementation of
// XRDeviceServiceHost
class XRDeviceServiceHostImpl : public device::mojom::XRDeviceServiceHost {
 public:
  XRDeviceServiceHostImpl()
      : gpu_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

  // BindGpu is called from the XR process to establish a connection to the GPU
  // process.
  void BindGpu(::mojo::PendingReceiver<::viz::mojom::Gpu> receiver) override {
    gpu_client_ = content::CreateGpuClient(std::move(receiver));
  }

 private:
  // The GpuClient associated with the XRDeviceService's GPU connection, if
  // any.
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;
};

}  // namespace

const mojo::Remote<device::mojom::XRDeviceService>& GetXRDeviceService() {
  static base::NoDestructor<mojo::Remote<device::mojom::XRDeviceService>>
      remote;
  if (!*remote) {
    content::ServiceProcessHost::Launch(
        remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("Isolated XR Device Service")
            .Pass());

    // Ensure that if the interface is ever disconnected (e.g. the service
    // process crashes) or goes idle for a short period of time -- meaning there
    // are no in-flight messages and no other interfaces bound through this
    // one -- then we will reset |remote|, causing the service process to be
    // terminated if it isn't already.
    remote->reset_on_disconnect();
    remote->reset_on_idle_timeout(base::Seconds(5));

    auto& startup_callback = GetStartupCallback();
    if (startup_callback)
      startup_callback.Run();
  }

  return *remote;
}

mojo::PendingRemote<device::mojom::XRDeviceServiceHost>
CreateXRDeviceServiceHost() {
  mojo::PendingRemote<device::mojom::XRDeviceServiceHost> device_service_host;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<XRDeviceServiceHostImpl>(),
      device_service_host.InitWithNewPipeAndPassReceiver());

  return device_service_host;
}

void SetXRDeviceServiceStartupCallbackForTestingInternal(
    base::RepeatingClosure callback) {
  GetStartupCallback() = std::move(callback);
}

}  // namespace content
