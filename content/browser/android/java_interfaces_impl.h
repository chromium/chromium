// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_INTERFACES_IMPL_H_
#define CONTENT_BROWSER_ANDROID_JAVA_INTERFACES_IMPL_H_

#include "content/public/browser/android/java_interfaces.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace content {
class RenderFrameHostImpl;
class WebContents;

// Returns an InterfaceProvider for global Java-implemented interfaces on the IO
// thread.
service_manager::InterfaceProvider* GetGlobalJavaInterfacesOnIOThread();

void BindInterfaceRegistryForWebContents(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
    WebContents* web_contents);

void BindInterfaceRegistryForRenderFrameHost(
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider> receiver,
    RenderFrameHostImpl* render_frame_host);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_INTERFACES_IMPL_H_
