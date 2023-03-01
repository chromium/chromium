// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state.mojom.h"

#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state_controller.mojom.h"
#include "url/origin.h"

namespace content {

// Implementation of mojo RuntimeFeatureStateController.
//
// This class handles API requests from the renderer process concerning runtime
// features that are enabled dynamically, most commonly from Origin Trials.
// The class will perform security checks before updating a RenderFrameHost's
// RuntimeFeatureStateReadContext with the validated feature state we receive.
class CONTENT_EXPORT RuntimeFeatureStateControllerImpl
    : public DocumentService<blink::mojom::RuntimeFeatureStateController> {
 public:
  RuntimeFeatureStateControllerImpl(const RuntimeFeatureStateControllerImpl&) =
      delete;
  RuntimeFeatureStateControllerImpl& operator=(
      const RuntimeFeatureStateControllerImpl&) = delete;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RuntimeFeatureStateController>
          receiver);

  // mojom::RuntimeFeatureStateController methods
  void ApplyFeatureDiffForOriginTrial(
      base::flat_map<::blink::mojom::RuntimeFeatureState,
                     ::blink::mojom::FeatureValuePtr> modified_features)
      override;
  void EnablePersistentTrial(
      const std::string& token,
      const std::vector<url::Origin>& script_origins) override;

 private:
  explicit RuntimeFeatureStateControllerImpl(
      RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::RuntimeFeatureStateController>
          receiver);

  // `this` can only be destructed as a DocumentService.
  ~RuntimeFeatureStateControllerImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RUNTIME_FEATURE_STATE_CONTROLLER_IMPL_H_
