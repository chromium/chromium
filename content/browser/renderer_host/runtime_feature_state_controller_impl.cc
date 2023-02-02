// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/runtime_feature_state_controller_impl.h"

#include "content/browser/bad_message.h"
#include "content/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state.mojom.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature_state_controller.mojom.h"

namespace content {

RuntimeFeatureStateControllerImpl::RuntimeFeatureStateControllerImpl(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::RuntimeFeatureStateController> receiver)
    : receiver_(this, std::move(receiver)), render_frame_host_(host) {}

RuntimeFeatureStateControllerImpl::~RuntimeFeatureStateControllerImpl() =
    default;

void RuntimeFeatureStateControllerImpl::ApplyFeatureDiffForOriginTrial(
    base::flat_map<::blink::mojom::RuntimeFeatureState,
                   ::blink::mojom::FeatureValuePtr> modified_features) {
  // Perform security checks by ensuring the following:
  base::flat_map<::blink::mojom::RuntimeFeatureState, bool>
      validated_features{};
  for (const auto& feature_pair : modified_features) {
    // Ensure the tokens we received are valid for this feature and origin.
    // TODO(https://crbug.com/1410784): add support for third-party Origin
    // Trials in the token validation process.
    std::string feature_name;
    blink::TrialTokenValidator validator;
    bool are_tokens_valid = true;
    for (const auto& token : feature_pair.second->tokens) {
      blink::TrialTokenResult result = validator.ValidateTokenAndTrial(
          token, render_frame_host_->GetLastCommittedOrigin(),
          base::Time::Now());
      if (result.Status() != blink::OriginTrialTokenStatus::kSuccess) {
        are_tokens_valid = false;
      } else {
        // All tokens should contain the same feature name. Store that name for
        // later validation checks.
        if (feature_name.empty()) {
          feature_name = result.ParsedToken()->feature_name();
        } else {
          DCHECK(feature_name == result.ParsedToken()->feature_name());
        }
      }
    }
    // We can add a feature to the RuntimeFeatureStateReadContext if:
    // 1. All of the tokens for the given feature were validated.
    // 2. The feature we received is an origin trial feature.
    // 3. The feature we received is expected in the browser process.
    if (are_tokens_valid) {
      // TODO(https://crbug.com/1410784): Since 3p tokens are not currently
      // supported, we cannot assume that invalid tokens are a sign of a
      // compromised renderer.
      if (blink::origin_trials::IsTrialValid(feature_name) &&
          blink::origin_trials::IsTrialEnabledForBrowserProcessReadWriteAccess(
              feature_name)) {
        validated_features[feature_pair.first] =
            feature_pair.second->is_enabled;
      } else {
        // The renderer is compromised so we terminate it.
        bad_message::ReceivedBadMessage(
            render_frame_host_->GetProcess(),
            bad_message::RFSCI_BROWSER_VALIDATION_BAD_ORIGIN_TRIAL_TOKEN);
      }
    }
  }
  // Apply the diff changes to the mutable RuntimeFeatureStateReadContext.
  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          &*render_frame_host_);
  document_data
      ->GetMutableRuntimeFeatureStateReadContext(
          base::PassKey<RuntimeFeatureStateControllerImpl>())
      .ApplyFeatureChange(validated_features);
}

}  // namespace content
