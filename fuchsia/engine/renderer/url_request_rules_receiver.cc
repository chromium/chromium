// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/url_request_rules_receiver.h"

#include "content/public/renderer/render_frame.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia/engine/url_request_rewrite.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

UrlRequestRulesReceiver::UrlRequestRulesReceiver(
    content::RenderFrame* render_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(render_frame);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedInterfaceRegistry, owned by |render_frame| will be torn-down at
  // the same time as |this|.
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(
          &UrlRequestRulesReceiver::OnUrlRequestRulesReceiverAssociatedReceiver,
          base::Unretained(this)));
}

UrlRequestRulesReceiver::~UrlRequestRulesReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>&
UrlRequestRulesReceiver::GetCachedRules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_rules_;
}

void UrlRequestRulesReceiver::OnUrlRequestRulesReceiverAssociatedReceiver(
    mojo::PendingAssociatedReceiver<mojom::UrlRequestRulesReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url_request_rules_receiver_.is_bound());
  url_request_rules_receiver_.Bind(std::move(receiver));
}

void UrlRequestRulesReceiver::OnRulesUpdated(
    std::vector<mojom::UrlRequestRulePtr> rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cached_rules_ =
      base::MakeRefCounted<WebEngineURLLoaderThrottle::UrlRequestRewriteRules>(
          std::move(rules));
}
