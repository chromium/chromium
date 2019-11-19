// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_AGENT_IMPL_H_
#define FUCHSIA_BASE_AGENT_IMPL_H_

#include <fuchsia/modular/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/zx/channel.h>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_provider_impl.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace cr_fuchsia {

// AgentImpl allows the set of services published to each component to be
// configured via a caller-supplied factory function for per-component state.
// Specializations of the ComponentStateBase returned by the factory function
// can extend it to publish specific services, and to manage per-component
// service state as desired.
class AgentImpl : public ::fuchsia::modular::Agent {
 public:
  // Common base for per-component services and state. The base provides an
  // outgoing directory into which specializations publish their services, to
  // have them made available to the client. Different specializations of the
  // ComponentStateBase can be created to suit different components.
  //
  // For example, to publish separate instances of Service1 to each component,
  // while having all components share a single instance of Service2:
  //
  // MyComponentState : public ComponentStateBase {
  //  public:
  //   MyComponentState(..) : ComponentStateBase(..)
  //       : binding1_(outgoing_directory(), &service1_),
  //         binding2_(outgoing_directory(), shared_service2_) {}
  //  private:
  //   Service1 service1_;
  //   ScopedServiceBinding<Service1> binding1_;
  //   Service2* shared_service2_; // e.g. Owned by the AgentImpl's embedder.
  //   ScopedServiceBinding<Service2> binding2_;
  // };
  class ComponentStateBase {
   public:
    virtual ~ComponentStateBase();

   protected:
    explicit ComponentStateBase(base::StringPiece component_id);

    // Returns the identity of the component served by this instance.
    const base::StringPiece component_id() const { return component_id_; }

    // Returns the directory into which the ComponentState implementation should
    // publish the services that the component may use.
    sys::OutgoingDirectory* outgoing_directory() {
      return &outgoing_directory_;
    }

    // Registers |service_binding| to prevent teardown of this
    // ComponentStateBase while it has one or more clients. |service_binding|
    // will typically be the ScopedServiceBinding<> of a critical service used
    // by the component.
    template <typename T>
    void AddKeepAliveBinding(T* service_binding) {
      keepalive_callbacks_.push_back(base::BindRepeating(
          &T::has_clients, base::Unretained(service_binding)));
      service_binding->SetOnLastClientCallback(base::BindRepeating(
          &ComponentStateBase::TeardownIfUnused, base::Unretained(this)));
    }

    // Requests that this instance be torn-down, regardless of whether one or
    // more keep-alive bindings (see above) have clients.
    void DisconnectClientsAndTeardown();

   private:
    friend class AgentImpl;

    // Tears down this instance if there are no ServiceProvider clients, and
    // no |keepalive_callbacks_| indicate that there are no clients of
    // bindings that were configured to keep us alive.
    void TeardownIfUnused();

    const std::string component_id_;
    AgentImpl* agent_impl_ = nullptr;
    sys::OutgoingDirectory outgoing_directory_;
    std::unique_ptr<base::fuchsia::ServiceProviderImpl> service_provider_;
    std::vector<base::RepeatingCallback<bool()>> keepalive_callbacks_;

    DISALLOW_COPY_AND_ASSIGN(ComponentStateBase);
  };

  // Creates a component state instance providing the services to which the
  // specified |component_id| should have access. Return nullptr to reject the
  // connection request.
  using CreateComponentStateCallback =
      base::RepeatingCallback<std::unique_ptr<ComponentStateBase>(
          base::StringPiece component_id)>;

  // Binds the Agent service in the supplied |outgoing_directory|, and invokes
  // |create_component_state_callback| on each Connect() call, for the caller to
  // create per-component data structures and services.
  AgentImpl(sys::OutgoingDirectory* outgoing_directory,
            CreateComponentStateCallback create_component_state_callback);
  ~AgentImpl() override;

  // fuchsia::modular::Agent implementation.
  void Connect(std::string requester_url,
               fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services)
      override;

 private:
  friend class ComponentStateBase;

  void DeleteComponentState(base::StringPiece component_id);
  void MaybeRunOnLastClientCallback();

  // Returns a ComponentStateBase instance for a given component-Id.
  const CreateComponentStateCallback create_component_state_callback_;

  // Binds this Agent implementation into the |outgoing_directory|.
  base::fuchsia::ScopedServiceBinding<::fuchsia::modular::Agent> agent_binding_;

  // Owns the ComponentState instances for each connected component.
  base::flat_map<std::string, std::unique_ptr<ComponentStateBase>>
      active_components_;

  DISALLOW_COPY_AND_ASSIGN(AgentImpl);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_AGENT_IMPL_H_
