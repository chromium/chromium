// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LOCAL_FRAME_HOST_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_LOCAL_FRAME_HOST_INTERCEPTOR_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace blink {
class AssociatedInterfaceProvider;
}  // namespace blink

namespace content {

// This class can be used to intercept mojo LocalFrameHost IPC messages being
// sent to the browser on the renderer side before they are sent to the browser.
class LocalFrameHostInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit LocalFrameHostInterceptor(blink::AssociatedInterfaceProvider*);
  ~LocalFrameHostInterceptor() override;

  blink::mojom::LocalFrameHost* GetForwardingInterface() override;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<blink::mojom::LocalFrameHost> receiver_{this};
  mojo::AssociatedRemote<blink::mojom::LocalFrameHost> local_frame_host_remote_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LOCAL_FRAME_HOST_INTERCEPTOR_H_
