// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/local_frame_host_interceptor.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

LocalFrameHostInterceptor::LocalFrameHostInterceptor(
    blink::AssociatedInterfaceProvider* provider) {
  provider->GetInterface(
      local_frame_host_remote_.BindNewEndpointAndPassReceiver());
  provider->OverrideBinderForTesting(
      blink::mojom::LocalFrameHost::Name_,
      base::BindRepeating(&LocalFrameHostInterceptor::BindFrameHostReceiver,
                          base::Unretained(this)));
}

LocalFrameHostInterceptor::~LocalFrameHostInterceptor() = default;

blink::mojom::LocalFrameHost*
LocalFrameHostInterceptor::GetForwardingInterface() {
  return local_frame_host_remote_.get();
}

void LocalFrameHostInterceptor::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::LocalFrameHost>(
      std::move(handle)));
}

}  // namespace content
