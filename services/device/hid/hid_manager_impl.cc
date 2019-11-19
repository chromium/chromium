// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/hid/hid_connection_impl.h"

namespace device {

base::LazyInstance<std::unique_ptr<HidService>>::Leaky g_hid_service =
    LAZY_INSTANCE_INITIALIZER;

HidManagerImpl::HidManagerImpl() : hid_service_observer_(this) {
  if (g_hid_service.Get())
    hid_service_ = std::move(g_hid_service.Get());
  else
    hid_service_ = HidService::Create();

  DCHECK(hid_service_);
  hid_service_observer_.Add(hid_service_.get());
}

HidManagerImpl::~HidManagerImpl() {}

// static
void HidManagerImpl::SetHidServiceForTesting(
    std::unique_ptr<HidService> hid_service) {
  g_hid_service.Get() = std::move(hid_service);
}

void HidManagerImpl::AddReceiver(
    mojo::PendingReceiver<mojom::HidManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void HidManagerImpl::GetDevicesAndSetClient(
    mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
    GetDevicesCallback callback) {
  hid_service_->GetDevices(base::BindOnce(
      &HidManagerImpl::CreateDeviceList, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(client)));
}

void HidManagerImpl::GetDevices(GetDevicesCallback callback) {
  hid_service_->GetDevices(base::BindOnce(
      &HidManagerImpl::CreateDeviceList, weak_factory_.GetWeakPtr(),
      std::move(callback), mojo::NullAssociatedRemote()));
}

void HidManagerImpl::CreateDeviceList(
    GetDevicesCallback callback,
    mojo::PendingAssociatedRemote<mojom::HidManagerClient> client,
    std::vector<mojom::HidDeviceInfoPtr> devices) {
  std::move(callback).Run(std::move(devices));

  if (!client.is_valid())
    return;

  clients_.Add(std::move(client));
}

void HidManagerImpl::Connect(
    const std::string& device_guid,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
    ConnectCallback callback) {
  hid_service_->Connect(device_guid,
                        base::AdaptCallbackForRepeating(base::BindOnce(
                            &HidManagerImpl::CreateConnection,
                            weak_factory_.GetWeakPtr(), std::move(callback),
                            std::move(connection_client), std::move(watcher))));
}

void HidManagerImpl::CreateConnection(
    ConnectCallback callback,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher,
    scoped_refptr<HidConnection> connection) {
  if (!connection) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::HidConnection> client;
  HidConnectionImpl::Create(connection, client.InitWithNewPipeAndPassReceiver(),
                            std::move(connection_client), std::move(watcher));
  std::move(callback).Run(std::move(client));
}

void HidManagerImpl::OnDeviceAdded(mojom::HidDeviceInfoPtr device) {
  for (auto& client : clients_)
    client->DeviceAdded(device->Clone());
}

void HidManagerImpl::OnDeviceRemoved(mojom::HidDeviceInfoPtr device) {
  for (auto& client : clients_)
    client->DeviceRemoved(device->Clone());
}

}  // namespace device
