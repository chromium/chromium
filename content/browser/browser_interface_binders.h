// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/device/public/mojom/battery_monitor.mojom-forward.h"
#include "services/device/public/mojom/vibration_manager.mojom-forward.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class DedicatedWorkerHost;
class SharedWorkerHost;
class SharedStorageWorkletHost;
class ServiceWorkerHost;
struct ServiceWorkerVersionInfo;
struct ServiceWorkerVersionBaseInfo;

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
void PopulateBinderMap(RenderFrameHostImpl* host, mojo::BinderMap* map);
void PopulateBinderMapWithContext(
    RenderFrameHostImpl* host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map);
RenderFrameHost* GetContextForHost(RenderFrameHostImpl* host);

// Registers the handlers for interfaces requested by dedicated workers.
void PopulateBinderMap(DedicatedWorkerHost* host, mojo::BinderMap* map);
void PopulateBinderMapWithContext(
    DedicatedWorkerHost* host,
    mojo::BinderMapWithContext<const url::Origin&>* map);
const url::Origin& GetContextForHost(DedicatedWorkerHost* host);

// Registers the handlers for interfaces requested by shared workers.
void PopulateBinderMap(SharedWorkerHost* host, mojo::BinderMap* map);
void PopulateBinderMapWithContext(
    SharedWorkerHost* host,
    mojo::BinderMapWithContext<const url::Origin&>* map);
url::Origin GetContextForHost(SharedWorkerHost* host);

// Registers the handlers for interfaces requested by shared storage worklets.
void PopulateBinderMap(SharedStorageWorkletHost* host, mojo::BinderMap* map);
void PopulateBinderMapWithContext(
    SharedStorageWorkletHost* host,
    mojo::BinderMapWithContext<SharedStorageWorkletHost*>* map);
SharedStorageWorkletHost* GetContextForHost(SharedStorageWorkletHost* host);

// Registers the handlers for interfaces requested by service workers.
void PopulateBinderMap(ServiceWorkerHost* host, mojo::BinderMap* map);
void PopulateBinderMapWithContext(
    ServiceWorkerHost* host,
    mojo::BinderMapWithContext<const ServiceWorkerVersionBaseInfo&>* map);
ServiceWorkerVersionInfo GetContextForHost(ServiceWorkerHost* host);

}  // namespace internal

// Allows tests to override how frame hosts bind BatteryMonitor receivers.
using BatteryMonitorBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::BatteryMonitor>)>;
CONTENT_EXPORT void OverrideBatteryMonitorBinderForTesting(
    BatteryMonitorBinder binder);

// Allows tests to override how frame hosts bind VibrationManager receivers.
using VibrationManagerBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<device::mojom::VibrationManager>,
    mojo::PendingRemote<device::mojom::VibrationManagerListener>)>;
CONTENT_EXPORT void OverrideVibrationManagerBinderForTesting(
    VibrationManagerBinder binder);

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BINDERS_H_
