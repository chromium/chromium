// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/fake_remote_main_frame_host.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace blink {

mojo::PendingAssociatedRemote<mojom::blink::RemoteMainFrameHost>
FakeRemoteMainFrameHost::BindNewAssociatedRemote() {
  receiver_.reset();
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

void FakeRemoteMainFrameHost::FocusPage() {}

void FakeRemoteMainFrameHost::TakeFocus(bool reverse) {}

void FakeRemoteMainFrameHost::UpdateTargetURL(
    const KURL&,
    mojom::blink::RemoteMainFrameHost::UpdateTargetURLCallback) {}

void FakeRemoteMainFrameHost::RouteCloseEvent() {}

}  // namespace blink
