// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_BASE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_BASE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/export.h"

namespace service_manager {

class LocalInterfaceProvider {
 public:
  // Binds |ptr| to an implementation of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(mojo::InterfacePtr<Interface>* ptr) {
    mojo::MessagePipe pipe;
    ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
    GetInterface(Interface::Name_, std::move(pipe.handle1));
  }
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request) {
    GetInterface(Interface::Name_, std::move(request.PassMessagePipe()));
  }
  template <typename Interface>
  void GetInterface(mojo::PendingReceiver<Interface> receiver) {
    GetInterface(Interface::Name_, std::move(receiver.PassPipe()));
  }
  virtual void GetInterface(const std::string& name,
                            mojo::ScopedMessagePipeHandle request_handle) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_PROVIDER_BASE_H_
