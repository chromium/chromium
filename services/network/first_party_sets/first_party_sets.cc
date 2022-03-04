// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets.h"

#include <initializer_list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/circular_deque.h"
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

FirstPartySets::FirstPartySets(bool enabled)
    : enabled_(enabled),
      pending_queries_(
          enabled ? std::make_unique<base::circular_deque<base::OnceClosure>>()
                  : nullptr) {
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(base::BindOnce(
      &FirstPartySets::SetCompleteSets, weak_factory_.GetWeakPtr()));
  if (!enabled)
    SetCompleteSets({});
}

FirstPartySets::~FirstPartySets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FirstPartySets::SetManuallySpecifiedSet(const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value())
    return;
  sets_loader_->SetManuallySpecifiedSet(flag_value);
}

void FirstPartySets::ParseAndSet(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value()) {
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
      FindOwnerInternal(site, infer_singleton_sets);
  if (!site_owner.has_value())
    return false;

  const auto is_owned_by_site_owner =
      [this, &site_owner,
       infer_singleton_sets](const net::SchemefulSite& context_site) -> bool {
    const FirstPartySets::OwnerResult context_owner =
        FindOwnerInternal(context_site, infer_singleton_sets);
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
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySets::ComputeMetadataAndInvoke, weak_factory_.GetWeakPtr(),
        site, top_frame_site, party_context, std::move(callback),
        base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return ComputeMetadataInternal(site, top_frame_site, party_context);
}

void FirstPartySets::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.ComputeMetadata",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(
      ComputeMetadataInternal(site, top_frame_site, party_context));
}

net::FirstPartySetMetadata FirstPartySets::ComputeMetadataInternal(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
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
      top_frame_site ? FindOwnerInternal(*top_frame_site,
                                         /*infer_singleton_sets=*/false)
                     : absl::nullopt;

  return net::FirstPartySetMetadata(context,
                                    base::OptionalOrNullptr(FindOwnerInternal(
                                        site, /*infer_singleton_sets=*/false)),
                                    base::OptionalOrNullptr(top_frame_owner),
                                    first_party_sets_context_type);
}

net::FirstPartySetsContextType FirstPartySets::ComputeContextType(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  constexpr bool infer_singleton_sets = true;
  const FirstPartySets::OwnerResult site_owner =
      FindOwnerInternal(site, infer_singleton_sets);
  // Note: the `party_context` consists of the intermediate frames (for frame
  // requests) or intermediate frames and current frame for subresource
  // requests.
  const bool is_homogeneous = base::ranges::all_of(
      party_context, [&](const net::SchemefulSite& middle_site) {
        return *FindOwnerInternal(middle_site, infer_singleton_sets) ==
               *site_owner;
      });
  if (top_frame_site == nullptr) {
    return is_homogeneous
               ? net::FirstPartySetsContextType::kTopFrameIgnoredHomogeneous
               : net::FirstPartySetsContextType::kTopFrameIgnoredMixed;
  }
  if (*FindOwnerInternal(*top_frame_site, infer_singleton_sets) != *site_owner)
    return net::FirstPartySetsContextType::kTopResourceMismatch;

  return is_homogeneous
             ? net::FirstPartySetsContextType::kHomogeneous
             : net::FirstPartySetsContextType::kTopResourceMatchMixed;
}

const FirstPartySets::OwnerResult FirstPartySets::FindOwnerInternal(
    const net::SchemefulSite& site,
    bool infer_singleton_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  const base::ElapsedTimer timer;

  net::SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();

  FirstPartySets::OwnerResult owner;
  const auto it = sets_->find(normalized_site);
  if (it != sets_->end()) {
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
    base::OnceCallback<void(FirstPartySets::OwnerResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySets::FindOwnerAndInvoke, weak_factory_.GetWeakPtr(), site,
        std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return FindOwnerInternal(site, /*infer_singleton_sets=*/false);
}

void FirstPartySets::FindOwnerAndInvoke(
    const net::SchemefulSite& site,
    base::OnceCallback<void(FirstPartySets::OwnerResult)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.FindOwner",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(
      FindOwnerInternal(site, /*infer_singleton_sets=*/false));
}

absl::optional<FirstPartySets::OwnersResult> FirstPartySets::FindOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySets::OwnersResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySets::FindOwnersAndInvoke, weak_factory_.GetWeakPtr(), sites,
        std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return FindOwnersInternal(sites);
}

void FirstPartySets::FindOwnersAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySets::OwnersResult)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.FindOwners",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(FindOwnersInternal(sites));
}

FirstPartySets::OwnersResult FirstPartySets::FindOwnersInternal(
    const base::flat_set<net::SchemefulSite>& sites) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  std::vector<std::pair<net::SchemefulSite, net::SchemefulSite>>
      sites_to_owners;
  for (const net::SchemefulSite& site : sites) {
    const FirstPartySets::OwnerResult owner =
        FindOwnerInternal(site, /*infer_singleton_sets=*/false);
    if (owner.has_value()) {
      sites_to_owners.emplace_back(site, owner.value());
    }
  }
  return sites_to_owners;
}

absl::optional<FirstPartySets::SetsByOwner> FirstPartySets::Sets(
    base::OnceCallback<void(FirstPartySets::SetsByOwner)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySets::SetsAndInvoke, weak_factory_.GetWeakPtr(),
        std::move(callback), base::TimeTicks::Now()));
    return absl::nullopt;
  }

  return SetsInternal();
}

void FirstPartySets::SetsAndInvoke(
    base::OnceCallback<void(FirstPartySets::SetsByOwner)> callback,
    base::TimeTicks enqueued_at) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UMA_HISTOGRAM_TIMES("Cookie.FirstPartySets.EnqueueingDelay.Sets",
                      base::TimeTicks::Now() - enqueued_at);

  std::move(callback).Run(SetsInternal());
}

FirstPartySets::SetsByOwner FirstPartySets::SetsInternal() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  FirstPartySets::SetsByOwner sets;

  for (const auto& pair : *sets_) {
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

void FirstPartySets::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());

  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadyToServeQueries",
      construction_timer_.Elapsed());

  if (!pending_queries_)
    return;

  base::UmaHistogramCounts10000("Cookie.FirstPartySets.DelayedQueriesCount",
                                pending_queries_->size());
  base::UmaHistogramTimes(
      "Cookie.FirstPartySets.MostDelayedQueryDelta",
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

void FirstPartySets::SetCompleteSets(FirstPartySets::FlattenedSets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sets_ = std::move(sets);
  ClearSiteDataOnChangedSetsIfReady();
  InvokePendingQueries();
}

void FirstPartySets::SetPersistedSetsAndOnSiteDataCleared(
    base::StringPiece raw_sets,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  raw_persisted_sets_ = static_cast<std::string>(raw_sets);
  on_site_data_cleared_ = std::move(callback);
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadPersistedSets",
      construction_timer_.Elapsed());
  ClearSiteDataOnChangedSetsIfReady();
}

void FirstPartySets::SetEnabledForTesting(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = enabled;
}

base::flat_set<net::SchemefulSite> FirstPartySets::ComputeSetsDiff(
    const FirstPartySets::FlattenedSets& old_sets) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sets_.has_value());
  if (old_sets.empty())
    return {};

  base::flat_set<net::SchemefulSite> result;
  for (const auto& old_pair : old_sets) {
    const net::SchemefulSite& old_member = old_pair.first;
    const net::SchemefulSite& old_owner = old_pair.second;
    const FirstPartySets::OwnerResult current_owner =
        FindOwnerInternal(old_member, /*infer_singleton_sets=*/false);
    // Look for the removed sites and the ones have owner changed.
    if (!current_owner || *current_owner != old_owner) {
      result.emplace(old_member);
    }
  }
  return result;
}

void FirstPartySets::ClearSiteDataOnChangedSetsIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!raw_persisted_sets_.has_value() || !sets_.has_value() ||
      on_site_data_cleared_.is_null()) {
    return;
  }

  base::flat_set<net::SchemefulSite> diff =
      ComputeSetsDiff(FirstPartySetParser::DeserializeFirstPartySets(
          raw_persisted_sets_.value()));

  // TODO(shuuran@chromium.org): Implement site state clearing.

  std::move(on_site_data_cleared_)
      .Run(FirstPartySetParser::SerializeFirstPartySets(*sets_));
}

void FirstPartySets::EnqueuePendingQuery(base::OnceClosure run_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!sets_.has_value());
  DCHECK(pending_queries_);

  if (!first_async_query_time_.has_value())
    first_async_query_time_ = {base::TimeTicks::Now()};

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network
