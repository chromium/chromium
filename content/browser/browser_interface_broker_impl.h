// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/browser_interface_binders.h"
#include "content/browser/mojo_binder_policy_applier.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

namespace content {

// content's implementation of the BrowserInterfaceBroker interface that binds
// interfaces requested by the renderer. Every execution context type (frame,
// worker etc) owns an instance and registers appropriate handlers, called
// "binders" (see internal::PopulateBinderMap and
// internal::PopulateBinderMapWithContext).
//
// By default, BrowserInterfaceBrokerImpl runs the binder that was registered
// for a given interface when the interface is requested. However, in some cases
// such as prerendering pages, it may be desirable to defer running the binder,
// or take another action. Setting a non-null `MojoBinderPolicyApplier` enables
// this behavior.
//
// Note: BrowserInterfaceBrokerImpl will eventually replace the usage of
// InterfaceProvider and browser manifests, as well as DocumentInterfaceBroker.
template <typename ExecutionContextHost, typename InterfaceBinderContext>
class BrowserInterfaceBrokerImpl : public blink::mojom::BrowserInterfaceBroker {
 public:
  explicit BrowserInterfaceBrokerImpl(ExecutionContextHost* host)
      : host_(host) {
    // The populate functions here define all the interfaces that will be
    // exposed through the broker.
    //
    // The `host` is a templated type (one of RenderFrameHostImpl,
    // ServiceWorkerHost, etc.). which allows the populate steps here to call a
    // set of overloaded functions based on that type. Thus each type of `host`
    // can expose a different set of interfaces, which is determined statically
    // at compile time.
    internal::PopulateBinderMap(host, &binder_map_);
    internal::PopulateBinderMapWithContext(host, &binder_map_with_context_);
  }

  // Disallows copy and move operations.
  BrowserInterfaceBrokerImpl(const BrowserInterfaceBrokerImpl& other) = delete;
  BrowserInterfaceBrokerImpl& operator=(
      const BrowserInterfaceBrokerImpl& other) = delete;
  BrowserInterfaceBrokerImpl(BrowserInterfaceBrokerImpl&&) = delete;
  BrowserInterfaceBrokerImpl& operator=(BrowserInterfaceBrokerImpl&&) = delete;

  // blink::mojom::BrowserInterfaceBroker
  void GetInterface(mojo::GenericPendingReceiver receiver) override {
    DCHECK(receiver.interface_name().has_value());
    if (!policy_applier_) {
      BindInterface(std::move(receiver));
    } else {
      std::string interface_name = receiver.interface_name().value();
      // base::Unretained is safe because `this` outlives `policy_applier_`.
      policy_applier_->ApplyPolicyToNonAssociatedBinder(
          interface_name,
          base::BindOnce(&BrowserInterfaceBrokerImpl::BindInterface,
                         base::Unretained(this), std::move(receiver)));
    }
  }

  // Sets MojoBinderPolicyApplier to control when to bind interfaces.
  void ApplyMojoBinderPolicies(MojoBinderPolicyApplier* policy_applier) {
    DCHECK(policy_applier);
    DCHECK(!policy_applier_);
    policy_applier_ = policy_applier;
  }

  // Stops applying policies to binding requests.
  void ReleaseMojoBinderPolicies() {
    DCHECK(policy_applier_);
    // Reset `policy_applier_` to disable capability control.
    policy_applier_ = nullptr;
  }

  void GetBinderMapInterfacesForTesting(std::vector<std::string>& out) {
    binder_map_.GetInterfacesForTesting(out);
    binder_map_with_context_.GetInterfacesForTesting(out);
  }

 private:
  void BindInterface(mojo::GenericPendingReceiver receiver) {
    if (!binder_map_.TryBind(&receiver)) {
      if (!binder_map_with_context_.TryBind(internal::GetContextForHost(host_),
                                            &receiver)) {
        host_->ReportNoBinderForInterface("No binder found for interface " +
                                          *receiver.interface_name());
      }
    }
  }

  const raw_ptr<ExecutionContextHost> host_;
  mojo::BinderMap binder_map_;
  mojo::BinderMapWithContext<InterfaceBinderContext> binder_map_with_context_;

  // The lifetime of `policy_applier_` is managed by the owner of this instance.
  // The owner should call `ReleaseMojoBinderPolicies()` when it destroys the
  // applier.
  raw_ptr<MojoBinderPolicyApplier> policy_applier_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
