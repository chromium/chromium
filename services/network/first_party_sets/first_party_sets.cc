// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets.h"

#include <initializer_list>
#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "services/network/first_party_sets/first_party_set_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

net::SamePartyContext::Type ContextTypeFromBool(bool is_same_party) {
  return is_same_party ? net::SamePartyContext::Type::kSameParty
                       : net::SamePartyContext::Type::kCrossParty;
}

}  // namespace

FirstPartySets::FirstPartySets(bool enabled) : enabled_(enabled) {
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(base::BindOnce(
      &FirstPartySets::SetCompleteSets, weak_factory_.GetWeakPtr()));
}

FirstPartySets::~FirstPartySets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FirstPartySets::SetManuallySpecifiedSet(const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enabled_)
    return;
  sets_loader_->SetManuallySpecifiedSet(flag_value);
}

void FirstPartySets::ParseAndSet(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enabled_) {
    sets_loader_->DisposeFile(std::move(sets_file));
    return;
  }
  sets_loader_->SetComponentSets(std::move(sets_file));
}

bool FirstPartySets::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    bool infer_singleton_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const FirstPartySets::OwnerResult site_owner =
      FindOwner(site, infer_singleton_sets);
  if (!site_owner.has_value())
    return false;

  const auto is_owned_by_site_owner =
      [this, &site_owner,
       infer_singleton_sets](const net::SchemefulSite& context_site) -> bool {
    const FirstPartySets::OwnerResult context_owner =
        FindOwner(context_site, infer_singleton_sets);
    return context_owner.has_value() && *context_owner == *site_owner;
  };

  if (top_frame_site && !is_owned_by_site_owner(*top_frame_site))
    return false;

  return base::ranges::all_of(party_context, is_owned_by_site_owner);
}

absl::optional<net::FirstPartySetMetadata> FirstPartySets::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::ElapsedTimer timer;

  net::SamePartyContext::Type context_type = ContextTypeFromBool(
      IsContextSamePartyWithSite(site, top_frame_site, party_context,
                                 /*infer_singleton_sets=*/false));
  net::SamePartyContext::Type ancestors = ContextTypeFromBool(
      IsContextSamePartyWithSite(site, top_frame_site, party_context,
                                 /*infer_singleton_sets=*/true));
  net::SamePartyContext::Type top_resource =
      ContextTypeFromBool(IsContextSamePartyWithSite(
          site, top_frame_site, {}, /*infer_singleton_sets=*/true));

  net::SamePartyContext context(context_type, ancestors, top_resource);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.ComputeContext.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);

  net::FirstPartySetsContextType first_party_sets_context_type =
      ComputeContextType(site, top_frame_site, party_context);

  FirstPartySets::OwnerResult top_frame_owner =
      top_frame_site
          ? FindOwner(*top_frame_site, /*infer_singleton_sets=*/false)
          : absl::nullopt;

  return net::FirstPartySetMetadata(
      context,
      base::OptionalOrNullptr(FindOwner(site, /*infer_singleton_sets=*/false)),
      base::OptionalOrNullptr(top_frame_owner), first_party_sets_context_type);
}

net::FirstPartySetsContextType FirstPartySets::ComputeContextType(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  constexpr bool infer_singleton_sets = true;
  const FirstPartySets::OwnerResult site_owner =
      FindOwner(site, infer_singleton_sets);
  // Note: the `party_context` consists of the intermediate frames (for frame
  // requests) or intermediate frames and current frame for subresource
  // requests.
  const bool is_homogeneous = base::ranges::all_of(
      party_context, [&](const net::SchemefulSite& middle_site) {
        return *FindOwner(middle_site, infer_singleton_sets) == *site_owner;
      });
  if (top_frame_site == nullptr) {
    return is_homogeneous
               ? net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous
               : net::FirstPartySetsContextType::kTopFrameIgnoredMixed;
  }
  if (*FindOwner(*top_frame_site, infer_singleton_sets) != *site_owner)
    return net::FirstPartySetsContextType::kTopResourceMismatch;

  return is_homogeneous
             ? net::FirstPartySetsContextType::kHomogeneous
             : net::FirstPartySetsContextType::kTopResourceMatchMixed;
}

const FirstPartySets::OwnerResult FirstPartySets::FindOwner(
    const net::SchemefulSite& site,
    bool infer_singleton_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::ElapsedTimer timer;

  net::SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();

  FirstPartySets::OwnerResult owner;
  const auto it = sets_.find(normalized_site);
  if (it != sets_.end()) {
    owner = it->second;
  } else if (infer_singleton_sets) {
    owner = normalized_site;
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.FindOwner.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);
  return owner;
}

absl::optional<FirstPartySets::OwnerResult> FirstPartySets::FindOwner(
    const net::SchemefulSite& site,
    base::OnceCallback<void(FirstPartySets::OwnerResult)> callback) const {
  return FindOwner(site, /*infer_singleton_sets=*/false);
}

absl::optional<FirstPartySets::OwnersResult> FirstPartySets::FindOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySets::OwnersResult)> callback) const {
  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>>
      sites_to_owners;
  for (const net::SchemefulSite& site : sites) {
    const FirstPartySets::OwnerResult owner =
        FindOwner(site, /*infer_singleton_sets=*/false);
    if (owner.has_value()) {
      sites_to_owners.emplace_back(site, owner.value());
    }
  }
  return sites_to_owners;
}

absl::optional<FirstPartySets::SetsByOwner> FirstPartySets::Sets(
    base::OnceCallback<void(FirstPartySets::SetsByOwner)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FirstPartySets::SetsByOwner sets;

  for (const auto& pair : sets_) {
    const net::SchemefulSite& member = pair.first;
    const net::SchemefulSite& owner = pair.second;
    auto set = sets.find(owner);
    if (set == sets.end()) {
      sets.emplace(owner, std::initializer_list<net::SchemefulSite>{member});
    } else {
      set->second.insert(member);
    }
  }

  return sets;
}

void FirstPartySets::SetCompleteSets(FirstPartySets::FlattenedSets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sets_ = std::move(sets);
  sets_ready_ = true;
  ClearSiteDataOnChangedSetsIfReady();
}

void FirstPartySets::SetPersistedSets(base::StringPiece raw_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  raw_persisted_sets_ = static_cast<std::string>(raw_sets);
  persisted_sets_ready_ = true;
  ClearSiteDataOnChangedSetsIfReady();
}

void FirstPartySets::SetOnSiteDataCleared(
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_site_data_cleared_ = std::move(callback);
  ClearSiteDataOnChangedSetsIfReady();
}

void FirstPartySets::SetEnabledForTesting(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = enabled;
}

base::flat_set<net::SchemefulSite> FirstPartySets::ComputeSetsDiff(
    const FirstPartySets::FlattenedSets& old_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (old_sets.empty())
    return {};

  base::flat_set<net::SchemefulSite> result;
  for (const auto& old_pair : old_sets) {
    const net::SchemefulSite& old_member = old_pair.first;
    const net::SchemefulSite& old_owner = old_pair.second;
    const FirstPartySets::OwnerResult current_owner =
        FindOwner(old_member, /*infer_singleton_sets=*/false);
    // Look for the removed sites and the ones have owner changed.
    if (!current_owner || *current_owner != old_owner) {
      result.emplace(old_member);
    }
  }
  return result;
}

void FirstPartySets::ClearSiteDataOnChangedSetsIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!persisted_sets_ready_ || !sets_ready_ || on_site_data_cleared_.is_null())
    return;

  base::flat_set<net::SchemefulSite> diff = ComputeSetsDiff(
      FirstPartySetParser::DeserializeFirstPartySets(raw_persisted_sets_));

  // TODO(shuuran@chromium.org): Implement site state clearing.

  std::move(on_site_data_cleared_)
      .Run(FirstPartySetParser::SerializeFirstPartySets(sets_));
}

}  // namespace network
