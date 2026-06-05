// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/types/optional_ref.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom-forward.h"

namespace network {

namespace {

bool IsEnabled(const mojom::FirstPartySetsAccessDelegateParamsPtr& params) {
  return params.is_null() || params->enabled;
}

}  // namespace

FirstPartySetsAccessDelegate::FirstPartySetsAccessDelegate(
    mojo::PendingReceiver<mojom::FirstPartySetsAccessDelegate> receiver,
    mojom::FirstPartySetsAccessDelegateParamsPtr params,
    FirstPartySetsManager* const manager)
    : manager_(manager),
      enabled_(IsEnabled(params)),
      ready_event_(receiver.is_valid() && manager->is_enabled()
                       ? std::nullopt
                       : std::make_optional(
                             network::mojom::FirstPartySetsReadyEvent::New())) {
  if (receiver.is_valid())
    receiver_.Bind(std::move(receiver));
}

FirstPartySetsAccessDelegate::~FirstPartySetsAccessDelegate() = default;

void FirstPartySetsAccessDelegate::NotifyReady(
    mojom::FirstPartySetsReadyEventPtr ready_event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ready_event_.has_value())
    return;
  ready_event_ = std::move(ready_event);
}

// TODO(crbug.com/1366846): Add metrics to track whether this is called from
// dynamic policy updates before NotifyReady.
void FirstPartySetsAccessDelegate::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = enabled;
}

std::pair<net::FirstPartySetMetadata, net::FirstPartySetsCacheFilter::MatchInfo>
FirstPartySetsAccessDelegate::ComputeMetadata(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_) {
    return std::make_pair(net::FirstPartySetMetadata(),
                          net::FirstPartySetsCacheFilter::MatchInfo());
  }
  if (!ready_event_.has_value()) {
    return std::make_pair(net::FirstPartySetMetadata(),
                          net::FirstPartySetsCacheFilter::MatchInfo());
  }

  net::FirstPartySetsCacheFilter::MatchInfo match_info(
      cache_filter()->GetMatchInfo(site));

  net::FirstPartySetMetadata metadata =
      manager_->ComputeMetadata(site, top_frame_site, *context_config());

  return std::make_pair(std::move(metadata), match_info);
}

}  // namespace network
