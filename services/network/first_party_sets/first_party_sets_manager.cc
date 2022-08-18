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
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
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
    SetCompleteSets(mojom::PublicFirstPartySets::New());
}

FirstPartySetsManager::~FirstPartySetsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FirstPartySetsManager::IsContextSamePartyWithSite(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const absl::optional<net::FirstPartySetEntry> site_entry =
      FindEntry(site, fps_context_config);
  if (!site_entry.has_value())
    return false;

  const auto is_in_same_set_as_frame_site =
      [this, &site_entry,
       &fps_context_config](const net::SchemefulSite& context_site) -> bool {
    const absl::optional<net::FirstPartySetEntry> context_entry =
        FindEntry(context_site, fps_context_config);
    return context_entry.has_value() &&
           context_entry->primary() == site_entry->primary();
  };

  if (top_frame_site && !is_in_same_set_as_frame_site(*top_frame_site))
    return false;

  return base::ranges::all_of(party_context, is_in_same_set_as_frame_site);
}

absl::optional<net::FirstPartySetMetadata>
FirstPartySetsManager::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::ComputeMetadataAndInvoke,
        weak_factory_.GetWeakPtr(), site, base::OptionalFromPtr(top_frame_site),
        party_context, fps_context_config, std::move(callback),
        base::ElapsedTimer()));
    return absl::nullopt;
  }

  return ComputeMetadataInternal(site, top_frame_site, party_context,
                                 fps_context_config);
}

void FirstPartySetsManager::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const absl::optional<net::SchemefulSite> top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback,
    base::ElapsedTimer timer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.ComputeMetadata",
                      timer.Elapsed());

  std::move(callback).Run(
      ComputeMetadataInternal(site, base::OptionalOrNullptr(top_frame_site),
                              party_context, fps_context_config));
}

net::FirstPartySetMetadata FirstPartySetsManager::ComputeMetadataInternal(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  DCHECK(fps_context_config.is_enabled());

  const base::ElapsedTimer timer;

  net::SamePartyContext::Type context_type =
      ContextTypeFromBool(IsContextSamePartyWithSite(
          site, top_frame_site, party_context, fps_context_config));

  net::SamePartyContext context(context_type);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.ComputeContext.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);

  absl::optional<net::FirstPartySetEntry> top_frame_owner =
      top_frame_site ? FindEntry(*top_frame_site, fps_context_config)
                     : absl::nullopt;

  return net::FirstPartySetMetadata(
      context, base::OptionalOrNullptr(FindEntry(site, fps_context_config)),
      base::OptionalOrNullptr(top_frame_owner));
}

absl::optional<net::FirstPartySetEntry> FirstPartySetsManager::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  DCHECK(fps_context_config.is_enabled());
  const base::ElapsedTimer timer;

  net::SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();

  absl::optional<net::FirstPartySetEntry> entry;

  if (is_enabled()) {
    // Check if `normalized_site` can be found in the customizations first.
    // If not, fall back to look up in `sets_`.
    if (const auto customizations_it =
            fps_context_config.customizations().find(normalized_site);
        customizations_it != fps_context_config.customizations().end()) {
      entry = customizations_it->second;
    } else {
      const auto canonical_it = aliases_.find(normalized_site);
      const net::SchemefulSite& canonical_site = canonical_it == aliases_.end()
                                                     ? normalized_site
                                                     : canonical_it->second;
      if (const auto sets_it = sets_->find(canonical_site);
          sets_it != sets_->end()) {
        entry = sets_it->second;
      }
    }
  }

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.FindOwner.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);
  return entry;
}

absl::optional<FirstPartySetsManager::OwnersResult>
FirstPartySetsManager::FindOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnersResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::FindOwnersAndInvoke, weak_factory_.GetWeakPtr(),
        sites, fps_context_config, std::move(callback), base::ElapsedTimer()));
    return absl::nullopt;
  }

  return FindOwnersInternal(sites, fps_context_config);
}

void FirstPartySetsManager::FindOwnersAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::OwnersResult)> callback,
    base::ElapsedTimer timer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.FindOwners",
                      timer.Elapsed());

  std::move(callback).Run(FindOwnersInternal(sites, fps_context_config));
}

FirstPartySetsManager::OwnersResult FirstPartySetsManager::FindOwnersInternal(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  DCHECK(fps_context_config.is_enabled());

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>>
      sites_to_entries;
  for (const net::SchemefulSite& site : sites) {
    const absl::optional<net::FirstPartySetEntry> entry =
        FindEntry(site, fps_context_config);
    if (entry.has_value()) {
      sites_to_entries.emplace_back(site, entry.value());
    }
  }
  return sites_to_entries;
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
  base::UmaHistogramTimes("Cookie.FirstPartySets.MostDelayedQueryDelta2",
                          first_async_query_timer_.has_value()
                              ? first_async_query_timer_->Elapsed()
                              : base::TimeDelta());

  while (!pending_queries_->empty()) {
    base::OnceClosure query_task = std::move(pending_queries_->front());
    pending_queries_->pop_front();
    std::move(query_task).Run();
  }

  pending_queries_ = nullptr;
}

void FirstPartySetsManager::SetCompleteSets(
    mojom::PublicFirstPartySetsPtr public_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value())
    return;
  sets_ = std::move(public_sets->sets);
  aliases_ = std::move(public_sets->aliases);
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

  if (!first_async_query_timer_.has_value())
    first_async_query_timer_ = {base::ElapsedTimer()};

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network
