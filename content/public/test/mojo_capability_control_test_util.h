// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOJO_CAPABILITY_CONTROL_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_MOJO_CAPABILITY_CONTROL_TEST_UTIL_H_

#include "content/public/browser/mojo_binder_policy_map.h"
#include "content/public/test/mojo_capability_control_test_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

class RenderFrameHost;

namespace test {

class MojoCapabilityControlTestHelper : mojom::TestInterfaceForDefer,
                                        mojom::TestInterfaceForGrant,
                                        mojom::TestInterfaceForCancel,
                                        mojom::TestInterfaceForUnexpected {
 public:
  MojoCapabilityControlTestHelper();
  ~MojoCapabilityControlTestHelper() override;
  MojoCapabilityControlTestHelper(const MojoCapabilityControlTestHelper&) =
      delete;
  MojoCapabilityControlTestHelper& operator=(
      const MojoCapabilityControlTestHelper&) = delete;

  void RegisterTestBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map);

  void RegisterTestMojoBinderPolicies(MojoBinderPolicyMap& policy_map);

  void GetInterface(RenderFrameHost* render_frame_host,
                    mojo::GenericPendingReceiver receiver);

  // mojom::TestInterfaceForDefer implementation.
  void Ping(PingCallback callback) override;

  size_t GetDeferReceiverSetSize();
  size_t GetGrantReceiverSetSize();

 private:
  void BindDeferInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForDefer> receiver);

  void BindGrantInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForGrant> receiver);

  void BindCancelInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForCancel> receiver);

  void BindUnexpectedInterface(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::TestInterfaceForUnexpected> receiver);

  mojo::ReceiverSet<mojom::TestInterfaceForDefer> defer_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForGrant> grant_receiver_set_;
  mojo::ReceiverSet<mojom::TestInterfaceForCancel> cancel_receiver_set_;
  mojo::Receiver<mojom::TestInterfaceForUnexpected> unexpected_receiver_{this};
};

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOJO_CAPABILITY_CONTROL_TEST_UTIL_H_
