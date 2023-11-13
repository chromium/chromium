// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mojo_capability_control_test_util.h"

#include "base/functional/bind.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"

namespace content::test {

MojoCapabilityControlTestHelper::MojoCapabilityControlTestHelper() = default;
MojoCapabilityControlTestHelper::~MojoCapabilityControlTestHelper() = default;

void MojoCapabilityControlTestHelper::
    RegisterTestBrowserInterfaceBindersForFrame(
        RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  map->Add<mojom::TestInterfaceForDefer>(
      base::BindRepeating(&MojoCapabilityControlTestHelper::BindDeferInterface,
                          base::Unretained(this)));
  map->Add<mojom::TestInterfaceForGrant>(
      base::BindRepeating(&MojoCapabilityControlTestHelper::BindGrantInterface,
                          base::Unretained(this)));
  map->Add<mojom::TestInterfaceForCancel>(
      base::BindRepeating(&MojoCapabilityControlTestHelper::BindCancelInterface,
                          base::Unretained(this)));
  map->Add<mojom::TestInterfaceForUnexpected>(base::BindRepeating(
      &MojoCapabilityControlTestHelper::BindUnexpectedInterface,
      base::Unretained(this)));
}

void MojoCapabilityControlTestHelper::RegisterTestMojoBinderPolicies(
    MojoBinderPolicyMap& policy_map) {
  policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForGrant>(
      MojoBinderNonAssociatedPolicy::kGrant);
  policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForCancel>(
      MojoBinderNonAssociatedPolicy::kCancel);
  policy_map.SetNonAssociatedPolicy<mojom::TestInterfaceForUnexpected>(
      MojoBinderNonAssociatedPolicy::kUnexpected);
}

void MojoCapabilityControlTestHelper::BindDeferInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::TestInterfaceForDefer> receiver) {
  defer_receiver_set_.Add(this, std::move(receiver));
}

void MojoCapabilityControlTestHelper::BindGrantInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::TestInterfaceForGrant> receiver) {
  grant_receiver_set_.Add(this, std::move(receiver));
}

void MojoCapabilityControlTestHelper::BindCancelInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::TestInterfaceForCancel> receiver) {
  cancel_receiver_set_.Add(this, std::move(receiver));
}

void MojoCapabilityControlTestHelper::BindUnexpectedInterface(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::TestInterfaceForUnexpected> receiver) {
  unexpected_receiver_.Bind(std::move(receiver));
}

void MojoCapabilityControlTestHelper::GetInterface(
    RenderFrameHost* render_frame_host,
    mojo::GenericPendingReceiver receiver) {
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>& bib =
      rfhi->browser_interface_broker_receiver_for_testing();
  blink::mojom::BrowserInterfaceBroker* controlled_broker =
      bib.internal_state()->impl();
  CHECK(controlled_broker);
  controlled_broker->GetInterface(std::move(receiver));
}

void MojoCapabilityControlTestHelper::Ping(PingCallback callback) {
  std::move(callback).Run();
}

size_t MojoCapabilityControlTestHelper::GetDeferReceiverSetSize() {
  return defer_receiver_set_.size();
}

size_t MojoCapabilityControlTestHelper::GetGrantReceiverSetSize() {
  return grant_receiver_set_.size();
}

}  // namespace content::test
