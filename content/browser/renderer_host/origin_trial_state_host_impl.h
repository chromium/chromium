// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ORIGIN_TRIAL_STATE_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_ORIGIN_TRIAL_STATE_HOST_IMPL_H_

#include "content/public/browser/document_service.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/origin_trial_state/origin_trial_state_host.mojom.h"
#include "url/origin.h"

namespace content {

class RenderFrameHost;

// Implementation of mojo OriginTrialStateHost.
//
// This class handles API requests from the renderer process concerning origin
// trials. The class will perform security checks before updating a
// RenderFrameHost's RuntimeFeatureStateReadContext with the validated feature
// state we receive.
class CONTENT_EXPORT OriginTrialStateHostImpl
    : public DocumentService<blink::mojom::OriginTrialStateHost> {
 public:
  OriginTrialStateHostImpl(const OriginTrialStateHostImpl&) = delete;
  OriginTrialStateHostImpl& operator=(const OriginTrialStateHostImpl&) = delete;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::OriginTrialStateHost> receiver);

  // mojom::OriginTrialStateHost methods
  void ApplyFeatureDiffForOriginTrial(
      base::flat_map<::blink::mojom::RuntimeFeature,
                     ::blink::mojom::OriginTrialFeatureStatePtr>
          origin_trial_features) override;
  void EnablePersistentTrial(
      const std::string& token,
      const std::vector<url::Origin>& script_origins) override;

 private:
  explicit OriginTrialStateHostImpl(
      RenderFrameHost& host,
      mojo::PendingReceiver<blink::mojom::OriginTrialStateHost> receiver);

  // `this` can only be destructed as a DocumentService.
  ~OriginTrialStateHostImpl() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ORIGIN_TRIAL_STATE_HOST_IMPL_H_
