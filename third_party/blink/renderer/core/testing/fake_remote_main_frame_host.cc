// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_remote_main_frame_host.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace blink {

void FakeRemoteMainFrameHost::Init(
    blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      mojom::blink::RemoteMainFrameHost::Name_,
      base::BindRepeating(
          &FakeRemoteMainFrameHost::BindRemoteMainFrameHostReceiver,
          base::Unretained(this)));
}

void FakeRemoteMainFrameHost::FocusPage() {}

void FakeRemoteMainFrameHost::UpdateTargetURL(
    const KURL&,
    mojom::blink::RemoteMainFrameHost::UpdateTargetURLCallback) {}

void FakeRemoteMainFrameHost::RouteCloseEvent() {}

void FakeRemoteMainFrameHost::BindRemoteMainFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(
      mojo::PendingAssociatedReceiver<mojom::blink::RemoteMainFrameHost>(
          std::move(handle)));
}

}  // namespace blink
