// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_

#include "content/browser/service_worker/service_worker_info.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class DedicatedWorkerHost;
class SharedWorkerHost;
class ServiceWorkerProviderHost;

namespace internal {

// PopulateBinderMap() registers BrowserInterfaceBroker's GetInterface()
// handler callbacks for different execution context types.
// An implementation of BrowserInterfaceBroker calls the relevant
// PopulateBinderMap() function passing its host execution context instance
// as the first argument and its interface name to handler map as the
// second one.
// This mechanism will replace interface registries and binders used for
// handling InterfaceProvider's GetInterface() calls (see crbug.com/718652).

// Registers the handlers for interfaces requested by frames.
void PopulateBinderMap(RenderFrameHostImpl* host,
                       service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    RenderFrameHostImpl* host,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map);
RenderFrameHost* GetContextForHost(RenderFrameHostImpl* host);

// Registers the handlers for interfaces requested by dedicated workers.
void PopulateBinderMap(DedicatedWorkerHost* host,
                       service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    DedicatedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map);
const url::Origin& GetContextForHost(DedicatedWorkerHost* host);

// Registers the handlers for interfaces requested by shared workers.
void PopulateBinderMap(SharedWorkerHost* host, service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    SharedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map);
url::Origin GetContextForHost(SharedWorkerHost* host);

// Registers the handlers for interfaces requested by service workers.
void PopulateBinderMap(ServiceWorkerProviderHost* host,
                       service_manager::BinderMap* map);
void PopulateBinderMapWithContext(
    ServiceWorkerProviderHost* host,
    service_manager::BinderMapWithContext<const ServiceWorkerVersionInfo&>*
        map);
ServiceWorkerVersionInfo GetContextForHost(ServiceWorkerProviderHost* host);

}  // namespace internal
}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
