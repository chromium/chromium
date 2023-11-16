// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_network/system_network_api.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/common/api/system_network.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

const char kNetworkListError[] = "Network lookup failed or unsupported";

}  // namespace

namespace extensions::api {

ExtensionFunction::ResponseAction
SystemNetworkGetNetworkInterfacesFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(
          &SystemNetworkGetNetworkInterfacesFunction::SendResponseOnUIThread,
          this));
  return RespondLater();
}

void SystemNetworkGetNetworkInterfacesFunction::SendResponseOnUIThread(
    const std::optional<net::NetworkInterfaceList>& interface_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!interface_list.has_value()) {
    Respond(Error(kNetworkListError));
    return;
  }

  std::vector<api::system_network::NetworkInterface> create_arg;
  create_arg.reserve(interface_list->size());
  for (const net::NetworkInterface& interface : *interface_list) {
    api::system_network::NetworkInterface info;
    info.name = interface.name;
    info.address = interface.address.ToString();
    info.prefix_length = interface.prefix_length;
    create_arg.push_back(std::move(info));
  }

  Respond(ArgumentList(
      api::system_network::GetNetworkInterfaces::Results::Create(create_arg)));
}

}  // namespace extensions::api
