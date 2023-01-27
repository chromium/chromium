// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_device_resource.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"

using content::BrowserThread;

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<UsbDeviceResource>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<UsbDeviceResource> >*
ApiResourceManager<UsbDeviceResource>::GetFactoryInstance() {
  return g_factory.Pointer();
}

UsbDeviceResource::UsbDeviceResource(
    const std::string& owner_extension_id,
    const std::string& guid,
    mojo::Remote<device::mojom::UsbDevice> device)
    : ApiResource(owner_extension_id), guid_(guid), device_(std::move(device)) {
  device_.set_disconnect_handler(base::BindOnce(
      &UsbDeviceResource::OnConnectionError, base::Unretained(this)));
}

UsbDeviceResource::~UsbDeviceResource() = default;

bool UsbDeviceResource::IsPersistent() const {
  return false;
}

void UsbDeviceResource::OnConnectionError() {
  device_.reset();
}

}  // namespace extensions
