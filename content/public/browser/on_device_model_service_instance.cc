// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/on_device_model_service_instance.h"

#include "base/no_destructor.h"
#include "content/public/browser/service_process_host.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace content {

// static
const mojo::Remote<on_device_model::mojom::OnDeviceModelService>&
GetRemoteOnDeviceModelService() {
  static base::NoDestructor<
      mojo::Remote<on_device_model::mojom::OnDeviceModelService>>
      service_remote([]() {
        mojo::Remote<on_device_model::mojom::OnDeviceModelService>
            service_remote;
        auto receiver = service_remote.BindNewPipeAndPassReceiver();
        service_remote.reset_on_disconnect();
        ServiceProcessHost::Launch<
            on_device_model::mojom::OnDeviceModelService>(
            std::move(receiver), ServiceProcessHost::Options()
                                     .WithDisplayName("On-Device Model Service")
                                     .Pass());
        return service_remote;
      }());

  return *service_remote;
}

}  // namespace content
