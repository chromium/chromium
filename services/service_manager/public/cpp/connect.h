// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECT_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECT_H_

#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace service_manager {

// Binds |receiver| to a remote implementation of Interface from |interfaces|.
template <typename Interface>
inline void GetInterface(mojom::InterfaceProvider* interfaces,
                         mojo::PendingReceiver<Interface> receiver) {
  interfaces->GetInterface(Interface::Name_, receiver.PassPipe());
}

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECT_H_
