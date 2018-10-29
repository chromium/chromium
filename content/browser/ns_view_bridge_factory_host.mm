// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ns_view_bridge_factory_host.h"

#include "base/no_destructor.h"

namespace content {

namespace {

using HostIdToFactoryMap = std::map<uint64_t, NSViewBridgeFactoryHost*>;

HostIdToFactoryMap* GetHostIdToFactoryMap() {
  static base::NoDestructor<HostIdToFactoryMap> instance;
  return instance.get();
}

}  // namespace

// static
const uint64_t NSViewBridgeFactoryHost::kLocalDirectHostId = -1;

// static
NSViewBridgeFactoryHost* NSViewBridgeFactoryHost::GetFromHostId(
    uint64_t host_id) {
  auto found = GetHostIdToFactoryMap()->find(host_id);
  if (found == GetHostIdToFactoryMap()->end())
    return nullptr;
  return found->second;
}

NSViewBridgeFactoryHost::NSViewBridgeFactoryHost(
    mojom::NSViewBridgeFactoryAssociatedRequest* request,
    uint64_t host_id)
    : host_id_(host_id) {
  *request = mojo::MakeRequest(&factory_);
  DCHECK(!GetHostIdToFactoryMap()->count(host_id_));
  GetHostIdToFactoryMap()->insert(std::make_pair(host_id_, this));
}

NSViewBridgeFactoryHost::~NSViewBridgeFactoryHost() {
  GetHostIdToFactoryMap()->erase(host_id_);
}

mojom::NSViewBridgeFactory* NSViewBridgeFactoryHost::GetFactory() {
  return factory_.get();
}

}  // namespace content
