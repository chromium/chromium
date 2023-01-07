// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_CONNECTOR_IMPL_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_CONNECTOR_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace service_manager {

// StandaloneConnectorImpl is a helper class which can be used to provide a
// backend for |Connector| objects to route requests somewhere other than the
// Service Manager.
//
// This exists to aid in transitioning code away from Service Manager APIs.
// Typically an instance of this class would live in the browser process, with a
// Delegate implementation that knows how to bind any interfaces that its
// clients might request.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) StandaloneConnectorImpl
    : private mojom::Connector {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked whenever a client asks to have an interface bound by the service
    // named |service_name|. The interface endpoint to bind is contained in
    // |receiver|.
    virtual void OnConnect(const std::string& service_name,
                           mojo::GenericPendingReceiver receiver) = 0;
  };

  explicit StandaloneConnectorImpl(Delegate* delegate);

  StandaloneConnectorImpl(const StandaloneConnectorImpl&) = delete;
  StandaloneConnectorImpl& operator=(const StandaloneConnectorImpl&) = delete;

  ~StandaloneConnectorImpl() override;

  // Produces a new remote Connector endpoint whose connection requests are
  // routed to this object's Delegate. The returned PendingRemote can be passed
  // around in messages, and it can eventually be used to construct a new
  // concrete Connector object.
  //
  // Note that Connectors bound to this remote or clones of it will only support
  // the basic operations of |BindInterface/Connect()|, and |Clone()|.
  mojo::PendingRemote<mojom::Connector> MakeRemote();

 private:
  // mojom::Connector implementation:
  void BindInterface(const ServiceFilter& filter,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     mojom::BindInterfacePriority priority,
                     BindInterfaceCallback callback) override;
  void QueryService(const std::string& service_name,
                    QueryServiceCallback callback) override;
  void WarmService(const ServiceFilter& filter,
                   WarmServiceCallback callback) override;
  void RegisterServiceInstance(
      const Identity& identity,
      mojo::ScopedMessagePipeHandle service_pipe,
      mojo::PendingReceiver<mojom::ProcessMetadata> metadata_receiver,
      RegisterServiceInstanceCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Connector> receiver) override;

  const raw_ptr<Delegate> delegate_;

  mojo::ReceiverSet<mojom::Connector> receivers_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_STANDALONE_CONNECTOR_IMPL_H_
