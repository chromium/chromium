// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vrp_flags/vrp_flags_factory_impl.h"

#include "base/no_destructor.h"
#include "components/vrp_flags/vrp_flags_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/network_service_instance.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

VrpFlagsFactoryImpl::VrpFlagsFactoryImpl() = default;
VrpFlagsFactoryImpl::~VrpFlagsFactoryImpl() = default;

// static
void VrpFlagsFactoryImpl::Bind(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlagsFactory> receiver) {
  static base::NoDestructor<VrpFlagsFactoryImpl> instance;
  instance->receivers_.Add(instance.get(), std::move(receiver));
}

void VrpFlagsFactoryImpl::BindBrowserVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  vrp_flags::VrpFlagsImpl::GetInstance()->Bind(std::move(receiver));
}

void VrpFlagsFactoryImpl::BindNetworkVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  GetNetworkService()->GetVrpFlags(base::BindOnce(
      [](mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver,
         mojo::PendingRemote<vrp_flags::mojom::VrpFlags> remote) {
        if (remote) {
          mojo::FusePipes(std::move(receiver), std::move(remote));
        }
      },
      std::move(receiver)));
}

void VrpFlagsFactoryImpl::BindGpuVrpFlags(
    mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver) {
  GpuProcessHost* gpu_host = GpuProcessHost::Get();
  if (!gpu_host) {
    receiver.ResetWithReason(0, "GPU Host is not available.");
    return;
  }
  gpu_host->gpu_service()->GetVrpFlags(base::BindOnce(
      [](mojo::PendingReceiver<vrp_flags::mojom::VrpFlags> receiver,
         mojo::PendingRemote<vrp_flags::mojom::VrpFlags> remote) {
        if (remote) {
          mojo::FusePipes(std::move(receiver), std::move(remote));
        }
      },
      std::move(receiver)));
}

}  // namespace content
