// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"

#include "fuchsia/engine/browser/url_request_rewrite_rules_validation.h"
#include "fuchsia/engine/url_request_rewrite_type_converters.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

// static
std::unique_ptr<UrlRequestRewriteRulesManager>
UrlRequestRewriteRulesManager::CreateForTesting() {
  return base::WrapUnique(new UrlRequestRewriteRulesManager());
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

UrlRequestRewriteRulesManager::~UrlRequestRewriteRulesManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

zx_status_t UrlRequestRewriteRulesManager::OnRulesUpdated(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rules,
    fuchsia::web::Frame::SetUrlRequestRewriteRulesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cached_rules_ = base::MakeRefCounted<url_rewrite::UrlRequestRewriteRules>(
      mojo::ConvertTo<mojom::UrlRequestRewriteRulesPtr>(std::move(rules)));
  if (!url_rewrite::ValidateRules(cached_rules_->data.get())) {
    cached_rules_ = nullptr;
    return ZX_ERR_INVALID_ARGS;
  }

  // Send the updated rules to the receivers.
  for (const auto& receiver_pair : active_remotes_) {
    receiver_pair.second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }

  // TODO(crbug.com/976975): Only call the callback when there are pending
  // throttles.
  std::move(callback)();
  return ZX_OK;
}

scoped_refptr<url_rewrite::UrlRequestRewriteRules>&
UrlRequestRewriteRulesManager::GetCachedRules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_rules_;
}

UrlRequestRewriteRulesManager::UrlRequestRewriteRulesManager() = default;

void UrlRequestRewriteRulesManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Register the frame rules receiver.
  mojo::AssociatedRemote<mojom::UrlRequestRulesReceiver> rules_receiver;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &rules_receiver);
  auto iter = active_remotes_.emplace(render_frame_host->GetGlobalId(),
                                      std::move(rules_receiver));
  DCHECK(iter.second);

  if (cached_rules_) {
    // Send an initial set of rules.
    iter.first->second->OnRulesUpdated(mojo::Clone(cached_rules_->data));
  }
}

void UrlRequestRewriteRulesManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t removed = active_remotes_.erase(render_frame_host->GetGlobalId());
  DCHECK_EQ(removed, 1u);
}
