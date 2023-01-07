// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_connection_resource.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<HidConnectionResource>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<HidConnectionResource> >*
ApiResourceManager<HidConnectionResource>::GetFactoryInstance() {
  return &g_factory.Get();
}

HidConnectionResource::HidConnectionResource(
    const std::string& owner_extension_id,
    mojo::PendingRemote<device::mojom::HidConnection> connection)
    : ApiResource(owner_extension_id), connection_(std::move(connection)) {}

HidConnectionResource::~HidConnectionResource() {
}

bool HidConnectionResource::IsPersistent() const {
  return false;
}

}  // namespace extensions
