// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_

#include "content/browser/browser_interface_binders.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

namespace content {

// content's implementation of the BrowserInterfaceBroker interface that binds
// interfaces requested by the renderer. Every execution context type (frame,
// worker etc) owns an instance and registers appropriate handlers (see
// internal::PopulateBinderMap).
// Note: this mechanism will eventually replace the usage of InterfaceProvider
// and browser manifests, as well as DocumentInterfaceBroker.
template <typename ExecutionContextHost, typename InterfaceBinderContext>
class BrowserInterfaceBrokerImpl : public blink::mojom::BrowserInterfaceBroker {
 public:
  BrowserInterfaceBrokerImpl(ExecutionContextHost* host) : host_(host) {
    internal::PopulateBinderMap(host, &binder_map_);
    internal::PopulateBinderMapWithContext(host, &binder_map_with_context_);
  }

  // blink::mojom::BrowserInterfaceBroker
  void GetInterface(mojo::GenericPendingReceiver receiver) {
    DCHECK(receiver.interface_name().has_value());
    auto interface_name = receiver.interface_name().value();
    auto pipe = receiver.PassPipe();
    if (!binder_map_.TryBind(interface_name, &pipe)) {
      binder_map_with_context_.TryBind(internal::GetContextForHost(host_),
                                       interface_name, &pipe);
    }
  }

 private:
  ExecutionContextHost* const host_;
  service_manager::BinderMap binder_map_;
  service_manager::BinderMapWithContext<InterfaceBinderContext>
      binder_map_with_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowserInterfaceBrokerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
