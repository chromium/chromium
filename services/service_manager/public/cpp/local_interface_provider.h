// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LOCAL_INTERFACE_PROVIDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LOCAL_INTERFACE_PROVIDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/export.h"

namespace service_manager {

class LocalInterfaceProvider {
 public:
  virtual ~LocalInterfaceProvider() = default;

  template <typename Interface>
  void GetInterface(mojo::PendingReceiver<Interface> receiver) {
    GetInterface(Interface::Name_, std::move(receiver.PassPipe()));
  }
  virtual void GetInterface(const std::string& name,
                            mojo::ScopedMessagePipeHandle request_handle) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_LOCAL_INTERFACE_PROVIDER_H_
