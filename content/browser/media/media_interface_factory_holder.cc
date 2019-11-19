// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_factory_holder.h"

#include "base/bind.h"
#include "content/public/common/service_manager_connection.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

MediaInterfaceFactoryHolder::MediaInterfaceFactoryHolder(
    const std::string& service_name,
    CreateInterfaceProviderCB create_interface_provider_cb)
    : service_name_(service_name),
      create_interface_provider_cb_(std::move(create_interface_provider_cb)) {}

MediaInterfaceFactoryHolder::~MediaInterfaceFactoryHolder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

media::mojom::InterfaceFactory* MediaInterfaceFactoryHolder::Get() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!interface_factory_remote_)
    ConnectToMediaService();

  return interface_factory_remote_.get();
}

void MediaInterfaceFactoryHolder::ConnectToMediaService() {
  media::mojom::MediaServicePtr media_service;

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(service_name_, &media_service);

  media_service->CreateInterfaceFactory(
      interface_factory_remote_.BindNewPipeAndPassReceiver(),
      create_interface_provider_cb_.Run());

  interface_factory_remote_.set_disconnect_handler(base::BindOnce(
      &MediaInterfaceFactoryHolder::OnMediaServiceConnectionError,
      base::Unretained(this)));
}

void MediaInterfaceFactoryHolder::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  interface_factory_remote_.reset();
}

}  // namespace content
