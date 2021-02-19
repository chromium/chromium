// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
#define CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_

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
  void GetInterface(mojo::GenericPendingReceiver receiver) {
    DCHECK(receiver.interface_name().has_value());
    if (!policy_applier_) {
      BindInterface(std::move(receiver));
    } else {
      std::string interface_name = receiver.interface_name().value();
      // base::Unretained is safe because `this` owns `policy_applier_`.
      policy_applier_->ApplyPolicyToBinder(
          interface_name,
          base::BindOnce(&BrowserInterfaceBrokerImpl::BindInterface,
                         base::Unretained(this), std::move(receiver)));
    }
  }

  // Sets MojoBinderPolicyApplier to control when to bind interfaces. Also,
  // discards the existing policy applier, if any, dropping any deferred
  // callbacks.
  void ApplyMojoBinderPolicies(
      std::unique_ptr<MojoBinderPolicyApplier> policy_applier) {
    DCHECK(blink::features::IsPrerender2Enabled());
    DCHECK(policy_applier);
    policy_applier_ = std::move(policy_applier);
  }

  // Resolves requests that were previously deferred and stops applying policies
  // to binding requests.
  void ReleaseMojoBinderPolicies() {
    // TODO(crbug.com/1174506): Temporary until we understand the cause of the
    // crash. Return to a DCHECK after the bug is fixed.
    CHECK(blink::features::IsPrerender2Enabled());
    // TODO(crbug.com/1174506): Temporary until we understand the cause of the
    // crash. Return to `DCHECK(policy_applier_)` after the bug is fixed.
    if (policy_applier_) {
      policy_applier_->GrantAll();
      // Reset `policy_applier_` to disable capability control.
      policy_applier_.reset();
    }
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

  ExecutionContextHost* const host_;
  mojo::BinderMap binder_map_;
  mojo::BinderMapWithContext<InterfaceBinderContext> binder_map_with_context_;
  std::unique_ptr<MojoBinderPolicyApplier> policy_applier_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_INTERFACE_BROKER_IMPL_H_
