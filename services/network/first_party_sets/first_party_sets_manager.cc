// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <initializer_list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

net::SamePartyContext::Type ContextTypeFromBool(bool is_same_party) {
  return is_same_party ? net::SamePartyContext::Type::kSameParty
                       : net::SamePartyContext::Type::kCrossParty;
}

}  // namespace

FirstPartySetsManager::FirstPartySetsManager(bool enabled)
    : enabled_(enabled),
      pending_queries_(
          enabled ? std::make_unique<base::circular_deque<base::OnceClosure>>()
                  : nullptr) {
  if (!enabled)
    SetCompleteSets({});
}

FirstPartySetsManager::~FirstPartySetsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FirstPartySetsManager::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config,
    bool infer_singleton_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const FirstPartySetsManager::OwnerResult site_owner =
      FindOwnerInternal(site, fps_context_config, infer_singleton_sets);
  if (!site_owner.has_value())
    return false;

  const auto is_owned_by_site_owner =
      [this, &site_owner, infer_singleton_sets,
       &fps_context_config](const net::SchemefulSite& context_site) -> bool {
    const FirstPartySetsManager::OwnerResult context_owner = FindOwnerInternal(
        context_site, fps_context_config, infer_singleton_sets);
    return context_owner.has_value() && *context_owner == *site_owner;
  };

  if (top_frame_site && !is_owned_by_site_owner(*top_frame_site))
    return false;

  return base::ranges::all_of(party_context, is_owned_by_site_owner);
}

absl::optional<net::FirstPartySetMetadata>
FirstPartySetsManager::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::ComputeMetadataAndInvoke,
        weak_factory_.GetWeakPtr(), site, base::OptionalFromPtr(top_frame_site),
        party_context, fps_context_config, std::move(callback),
        base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return ComputeMetadataInternal(site, top_frame_site, party_context,
                                 fps_context_config);
}

void FirstPartySetsManager::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const absl::optional<net::SchemefulSite> top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.ComputeMetadata",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(
      ComputeMetadataInternal(site, base::OptionalOrNullptr(top_frame_site),
                              party_context, fps_context_config));
}

net::FirstPartySetMetadata FirstPartySetsManager::ComputeMetadataInternal(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  const base::ElapsedTimer timer;

  net::SamePartyContext::Type context_type =
      ContextTypeFromBool(IsContextSamePartyWithSite(
          site, top_frame_site, party_context, fps_context_config,
          /*infer_singleton_sets=*/false));
  net::SamePartyContext::Type ancestors =
      ContextTypeFromBool(IsContextSamePartyWithSite(
          site, top_frame_site, party_context, fps_context_config,
          /*infer_singleton_sets=*/true));
  net::SamePartyContext::Type top_resource = ContextTypeFromBool(
      IsContextSamePartyWithSite(site, top_frame_site, {}, fps_context_config,
                                 /*infer_singleton_sets=*/true));

  net::SamePartyContext context(context_type, ancestors, top_resource);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.ComputeContext.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);

  net::FirstPartySetsContextType first_party_sets_context_type =
      ComputeContextType(site, top_frame_site, party_context,
                         fps_context_config);

  FirstPartySetsManager::OwnerResult top_frame_owner =
      top_frame_site ? FindOwnerInternal(*top_frame_site, fps_context_config,
                                         /*infer_singleton_sets=*/false)
                     : absl::nullopt;

  return net::FirstPartySetMetadata(
      context,
      base::OptionalOrNullptr(FindOwnerInternal(
          site, fps_context_config, /*infer_singleton_sets=*/false)),
      base::OptionalOrNullptr(top_frame_owner), first_party_sets_context_type);
}

net::FirstPartySetsContextType FirstPartySetsManager::ComputeContextType(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  constexpr bool infer_singleton_sets = true;
  const FirstPartySetsManager::OwnerResult site_owner =
      FindOwnerInternal(site, fps_context_config, infer_singleton_sets);
  // Note: the `party_context` consists of the intermediate frames (for frame
  // requests) or intermediate frames and current frame for subresource
  // requests.
  const bool is_homogeneous = base::ranges::all_of(
      party_context, [&](const net::SchemefulSite& middle_site) {
        return *FindOwnerInternal(middle_site, fps_context_config,
                                  infer_singleton_sets) == *site_owner;
      });
  if (top_frame_site == nullptr) {
    return is_homogeneous
               ? net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous
               : net::FirstPartySetsContextType::kTopFrameIgnoredMixed;
  }
  if (*FindOwnerInternal(*top_frame_site, fps_context_config,
                         infer_singleton_sets) != *site_owner)
    return net::FirstPartySetsContextType::kTopResourceMismatch;

  return is_homogeneous
             ? net::FirstPartySetsContextType::kHomogeneous
             : net::FirstPartySetsContextType::kTopResourceMatchMixed;
}

const FirstPartySetsManager::OwnerResult
FirstPartySetsManager::FindOwnerInternal(
    const net::SchemefulSite& site,
    const FirstPartySetsContextConfig& fps_context_config,
    bool infer_singleton_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  const base::ElapsedTimer timer;

  net::SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();

  FirstPartySetsManager::OwnerResult owner;

  if (fps_context_config.is_enabled() && is_enabled()) {
    // Check if `normalized_site` can be found in the customizations first.
    // If not, fall back to look up in `sets_`.
    if (const auto it =
            fps_context_config.customizations().find(normalized_site);
        it != fps_context_config.customizations().end()) {
      owner = it->second;
    } else if (const auto it = sets_->find(normalized_site);
               it != sets_->end()) {
      owner = it->second;
    }
  }

  if (!owner.has_value() && infer_singleton_sets) {
    owner = normalized_site;
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.FindOwner.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);
  return owner;
}

absl::optional<FirstPartySetsManager::OwnerResult>
FirstPartySetsManager::FindOwner(
    const net::SchemefulSite& site,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnerResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::FindOwnerAndInvoke, weak_factory_.GetWeakPtr(),
        site, fps_context_config, std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return FindOwnerInternal(site, fps_context_config,
                           /*infer_singleton_sets=*/false);
}

void FirstPartySetsManager::FindOwnerAndInvoke(
    const net::SchemefulSite& site,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnerResult)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.FindOwner",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(FindOwnerInternal(site, fps_context_config,
                                            /*infer_singleton_sets=*/false));
}

absl::optional<FirstPartySetsManager::OwnersResult>
FirstPartySetsManager::FindOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnersResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsManager::FindOwnersAndInvoke,
                       weak_factory_.GetWeakPtr(), sites, fps_context_config,
                       std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return FindOwnersInternal(sites, fps_context_config);
}

void FirstPartySetsManager::FindOwnersAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnersResult)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.FindOwners",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(FindOwnersInternal(sites, fps_context_config));
}

FirstPartySetsManager::OwnersResult FirstPartySetsManager::FindOwnersInternal(
    const base::flat_set<net::SchemefulSite>& sites,
    const FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  if (!fps_context_config.is_enabled())
    return {};

  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>>
      sites_to_owners;
  for (const net::SchemefulSite& site : sites) {
    const FirstPartySetsManager::OwnerResult owner = FindOwnerInternal(
        site, fps_context_config, /*infer_singleton_sets=*/false);
    if (owner.has_value()) {
      sites_to_owners.emplace_back(site, owner.value());
    }
  }
  return sites_to_owners;
}

absl::optional<FirstPartySetsManager::SetsByOwner> FirstPartySetsManager::Sets(
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::SetsByOwner)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::SetsAndInvoke, weak_factory_.GetWeakPtr(),
        fps_context_config, std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return SetsInternal(fps_context_config);
}

void FirstPartySetsManager::SetsAndInvoke(
    const FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::SetsByOwner)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.Sets",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(SetsInternal(fps_context_config));
}

FirstPartySetsManager::SetsByOwner FirstPartySetsManager::SetsInternal(
    const FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  if (!fps_context_config.is_enabled() || !is_enabled())
    return {};

  FirstPartySetsManager::SetsByOwner sets;
  // Go over `sets_` to add entries that are not modified by the customizations.
  for (const auto& pair : *sets_) {
    const net::SchemefulSite& member = pair.first;
    const net::SchemefulSite& owner = pair.second;
    if (!base::Contains(fps_context_config.customizations(), member)) {
      auto set = sets.emplace(owner, std::set<net::SchemefulSite>()).first;
      set->second.insert(member);
    }
  }

  // Then go over the customizations to add entries that are not deleted.
  for (const auto& pair : fps_context_config.customizations()) {
    const net::SchemefulSite& member = pair.first;
    const absl::optional<net::SchemefulSite>& owner = pair.second;
    if (owner.has_value()) {
      auto set =
          sets.emplace(std::move(owner.value()), std::set<net::SchemefulSite>())
              .first;
      set->second.insert(member);
    }
  }

  return sets;
}

void FirstPartySetsManager::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadyToServeQueries2",
      construction_timer_.Elapsed());

  if (!pending_queries_)
    return;

  base::UmaHistogramCounts10000("Cookie.FirstPartySets.DelayedQueriesCount",
                                pending_queries_->size());
  base::UmaHistogramTimes(
      "Cookie.FirstPartySets.MostDelayedQueryDelta2",
      first_async_query_time_.has_value()
          ? base::TimeTicks::Now() - first_async_query_time_.value()
          : base::TimeDelta());

  while (!pending_queries_->empty()) {
    base::OnceClosure query_task = std::move(pending_queries_->front());
    pending_queries_->pop_front();
    std::move(query_task).Run();
  }

  pending_queries_ = nullptr;
}

void FirstPartySetsManager::SetCompleteSets(
    FirstPartySetsManager::FlattenedSets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value())
    return;
  sets_ = std::move(sets);
  InvokePendingQueries();
}

void FirstPartySetsManager::SetEnabledForTesting(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = enabled;
}

void FirstPartySetsManager::EnqueuePendingQuery(base::OnceClosure run_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!sets_.has_value());
  DCHECK(pending_queries_);

  if (!first_async_query_time_.has_value())
    first_async_query_time_ = {base::TimeTicks::Now()};

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network
