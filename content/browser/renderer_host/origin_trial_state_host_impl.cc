// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/origin_trial_state_host_impl.h"

#include "content/browser/bad_message.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/origin_trial_state/origin_trial_state_host.mojom.h"

namespace content {

OriginTrialStateHostImpl::OriginTrialStateHostImpl(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::OriginTrialStateHost> receiver)
    : DocumentService(host, std::move(receiver)) {}

OriginTrialStateHostImpl::~OriginTrialStateHostImpl() = default;

// static
void OriginTrialStateHostImpl::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::OriginTrialStateHost> receiver) {
  CHECK(host);
  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new OriginTrialStateHostImpl(*host, std::move(receiver));
}

void OriginTrialStateHostImpl::ApplyFeatureDiffForOriginTrial(
    base::flat_map<::blink::mojom::RuntimeFeature,
                   ::blink::mojom::OriginTrialFeatureStatePtr>
        origin_trial_features) {
  // TODO(crbug.com/40243430): RuntimeFeatureState does not yet support
  // HTTP header origin trial tokens, which currently cause this function to be
  // called between RenderFrameHostImpl::CommitNavigation() and
  // RenderFrameHostImpl::DidCommitNavigation(). As a result, we will reject all
  // tokens that are sent before the navigation has committed, as we cannot
  // validate them.
  if (render_frame_host().GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPendingCommit) {
    return;
  }
  // Perform security checks by ensuring the following:
  base::flat_map<::blink::mojom::RuntimeFeature, bool> validated_features{};
  base::flat_map<::blink::mojom::RuntimeFeature, std::vector<std::string>>
      possible_third_party_features{};
  for (const auto& feature_pair : origin_trial_features) {
    // Ensure the tokens we received are valid for this feature and origin.
    std::string feature_name;
    blink::TrialTokenValidator validator;
    bool are_tokens_valid = true;
    for (const auto& token : feature_pair.second->tokens) {
      // Third party tokens will be rejected as invalid here. These will instead
      // be collected in `possible_third_party_features` for later validation
      // via Is$FEATURE$EnabledForThirdParty.
      blink::TrialTokenResult result = validator.ValidateTokenAndTrial(
          token, render_frame_host().GetLastCommittedOrigin(),
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
      if (blink::origin_trials::IsTrialValid(feature_name) &&
          blink::origin_trials::IsTrialEnabledForBrowserProcessReadAccess(
              feature_name)) {
        validated_features[feature_pair.first] =
            feature_pair.second->is_enabled;
      } else {
        // The renderer is compromised so we terminate it.
        bad_message::ReceivedBadMessage(
            render_frame_host().GetProcess(),
            bad_message::RFSCI_BROWSER_VALIDATION_BAD_ORIGIN_TRIAL_TOKEN);
        return;
      }
    } else if (feature_pair.second->is_enabled) {
      // If we could not validate the tokens it's possible there's a third-party
      // origin trial among them. In this case we should store the tokens for
      // later validation once the potential third-party origin is known.
      possible_third_party_features[feature_pair.first] =
          feature_pair.second->tokens;
    }
  }
  // Apply the diff changes to the mutable RuntimeFeatureStateReadContext.
  // TODO(crbug.com/347186599): CAVEAT EMPTOR - there are corner cases where
  // RuntimeFeatureStateDocumentData::GetForCurrentDocument() returned a nullptr
  // when it shouldn't have. To prevent CHECK failures, we will create a new
  // RuntimeFeatureStateDocumentData, but this does not resolve the original
  // corner case where the DocumentData is incorrectly created/deleted.
  // This issue should be revisited to avoid silently dropping any feature
  // overrides that are stored in the RFSDocumentData, in these corner cases
  // when the data has become a nullptr.
  RuntimeFeatureStateDocumentData* document_data =
      RuntimeFeatureStateDocumentData::GetForCurrentDocument(
          &render_frame_host());
  if (!document_data) {
    // We can't use
    // RuntimeFeatureStateDocumentData::GetOrCreateForCurrentDocument() because
    // that creates an empty RuntimeFeatureStateReadContext which will hit some
    // internal CHECKs if used because all its member fields are empty. Passing
    // in a RuntimeFeatureStateContext() will initialize those member fields.
    RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        &render_frame_host(), blink::RuntimeFeatureStateContext());
    document_data = RuntimeFeatureStateDocumentData::GetForCurrentDocument(
        &render_frame_host());
  }
  CHECK(document_data);
  document_data
      ->GetMutableRuntimeFeatureStateReadContext(
          base::PassKey<OriginTrialStateHostImpl>())
      .ApplyFeatureChange(validated_features, possible_third_party_features);
}

void OriginTrialStateHostImpl::EnablePersistentTrial(
    const std::string& token,
    const std::vector<url::Origin>& script_origins) {
  OriginTrialsControllerDelegate* delegate =
      render_frame_host()
          .GetBrowserContext()
          ->GetOriginTrialsControllerDelegate();
  if (!delegate) {
    return;
  }

  // No validation required here, as the delegate will fully validate the
  // provided token.
  std::vector<std::string> tokens = {token};
  delegate->PersistAdditionalTrialsFromTokens(
      /*origin=*/render_frame_host().GetLastCommittedOrigin(),
      /*partition_origin=*/
      render_frame_host().GetOutermostMainFrame()->GetLastCommittedOrigin(),
      script_origins, tokens, base::Time::Now(),
      render_frame_host().GetPageUkmSourceId());
}

}  // namespace content
