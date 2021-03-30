// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/xr_device_service.h"

#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/service_sandbox_type.h"
#include "content/public/browser/browser_task_traits.h"
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
    gpu_client_ =
        content::CreateGpuClient(std::move(receiver), base::DoNothing(),
                                 content::GetIOThreadTaskRunner({}));
  }

 private:
  // The GpuClient associated with the XRDeviceService's GPU connection, if
  // any.
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;
};

void BindHost(
    mojo::PendingReceiver<device::mojom::XRDeviceServiceHost> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<XRDeviceServiceHostImpl>(),
                              std::move(receiver));
}

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
    remote->reset_on_idle_timeout(base::TimeDelta::FromSeconds(5));

    auto& startup_callback = GetStartupCallback();
    if (startup_callback)
      startup_callback.Run();
  }

  return *remote;
}

mojo::PendingRemote<device::mojom::XRDeviceServiceHost>
CreateXRDeviceServiceHost() {
  // XRDeviceServiceHostImpl doesn't need to live on the IO thread but GpuClient
  // does and it will own GpuClient. Might as well have them both live on the IO
  // thread.
  mojo::PendingRemote<device::mojom::XRDeviceServiceHost> device_service_host;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BindHost,
                     device_service_host.InitWithNewPipeAndPassReceiver()));

  return device_service_host;
}

void SetXRDeviceServiceStartupCallbackForTestingInternal(
    base::RepeatingClosure callback) {
  GetStartupCallback() = std::move(callback);
}

}  // namespace content
