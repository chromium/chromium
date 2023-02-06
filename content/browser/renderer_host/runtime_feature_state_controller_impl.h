// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state.mojom.h"

#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state_controller.mojom.h"

namespace content {

// Implementation of mojo RuntimeFeatureStateController.
// This class handles API requests from the renderer process, performing
// security checks before updating a RenderFrameHost's
// RuntimeFeatureStateReadContext with the validated feature state we receive.
// An instance of this class is owned by the RenderFrameHostImpl. It is
// instantiated on-demand via the BrowserInterfaceBroker once the renderer
// creates and binds a remote instance.
class RuntimeFeatureStateControllerImpl
    : public blink::mojom::RuntimeFeatureStateController {
 public:
  // Constructor takes both the RenderFrameHost and the receiver. The document
  // data may be altered by a future IPC call.
  explicit RuntimeFeatureStateControllerImpl(
      RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::RuntimeFeatureStateController>
          receiver);

  ~RuntimeFeatureStateControllerImpl() override;

  // mojom::RuntimeFeatureStateController methods
  void ApplyFeatureDiffForOriginTrial(
      base::flat_map<::blink::mojom::RuntimeFeatureState,
                     ::blink::mojom::FeatureValuePtr> modified_features)
      override;

 private:
  mojo::Receiver<blink::mojom::RuntimeFeatureStateController> receiver_;
  raw_ref<RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_
