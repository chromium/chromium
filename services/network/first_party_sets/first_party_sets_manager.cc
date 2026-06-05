// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/types/optional_ref.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace network {

FirstPartySetsManager::FirstPartySetsManager(bool enabled) : enabled_(enabled) {
  if (!enabled)
    SetCompleteSets(net::GlobalFirstPartySets());
}

FirstPartySetsManager::~FirstPartySetsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

net::FirstPartySetMetadata FirstPartySetsManager::ComputeMetadata(
    const net::SchemefulSite& site,
    base::optional_ref<const net::SchemefulSite> top_frame_site,
    const net::FirstPartySetsContextConfig& fps_context_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    return net::FirstPartySetMetadata();
  }

  return sets_->ComputeMetadata(site, top_frame_site, fps_context_config);
}

void FirstPartySetsManager::SetCompleteSets(net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value())
    return;
  sets_ = std::move(sets);
}

}  // namespace network
