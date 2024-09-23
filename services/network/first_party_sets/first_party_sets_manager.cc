// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_manager.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_util.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace network {

FirstPartySetsManager::FirstPartySetsManager(bool enabled)
    : enabled_(enabled),
      wait_for_init_(base::FeatureList::IsEnabled(
          net::features::kWaitForFirstPartySetsInit)),
      pending_queries_(
          enabled && wait_for_init_
              ? std::make_unique<base::circular_deque<base::OnceClosure>>()
              : nullptr) {
  if (!enabled)
    SetCompleteSets(net::GlobalFirstPartySets());
}

FirstPartySetsManager::~FirstPartySetsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<net::FirstPartySetMetadata>
FirstPartySetsManager::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    if (!wait_for_init_) {
      return net::FirstPartySetMetadata();
    }
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsManager::ComputeMetadataAndInvoke,
        weak_factory_.GetWeakPtr(), site, base::OptionalFromPtr(top_frame_site),
        fps_context_config.Clone(), std::move(callback)));
    return std::nullopt;
  }

  return ComputeMetadataInternal(site, top_frame_site, fps_context_config);
}

void FirstPartySetsManager::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const std::optional<net::SchemefulSite> top_frame_site,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());

  std::move(callback).Run(ComputeMetadataInternal(
      site, base::OptionalToPtr(top_frame_site), fps_context_config));
}

net::FirstPartySetMetadata FirstPartySetsManager::ComputeMetadataInternal(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());

  return sets_->ComputeMetadata(site, top_frame_site, fps_context_config);
}

std::optional<net::FirstPartySetEntry> FirstPartySetsManager::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());
  const base::ElapsedTimer timer;

  std::optional<net::FirstPartySetEntry> entry =
      is_enabled() ? sets_->FindEntry(site, fps_context_config) : std::nullopt;

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.FindOwner.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);
  return entry;
}

std::optional<FirstPartySetsManager::EntriesResult>
FirstPartySetsManager::FindEntries(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::EntriesResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sets_.has_value()) {
    if (!wait_for_init_) {
      return FirstPartySetsManager::EntriesResult();
    }
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsManager::FindEntriesAndInvoke,
                       weak_factory_.GetWeakPtr(), sites,
                       fps_context_config.Clone(), std::move(callback)));
    return std::nullopt;
  }

  return FindEntriesInternal(sites, fps_context_config);
}

void FirstPartySetsManager::FindEntriesAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config,
    base::OnceCallback<void(FirstPartySetsManager::EntriesResult)> callback)
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());

  std::move(callback).Run(FindEntriesInternal(sites, fps_context_config));
}

FirstPartySetsManager::EntriesResult FirstPartySetsManager::FindEntriesInternal(
    const base::flat_set<net::SchemefulSite>& sites,
    const net::FirstPartySetsContextConfig& fps_context_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());

  return sets_->FindEntries(sites, fps_context_config);
}

void FirstPartySetsManager::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(sets_.has_value());

  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadyToServeQueries2",
      construction_timer_.Elapsed());

  base::UmaHistogramTimes("Cookie.FirstPartySets.Network.MostDelayedQueryDelta",
                          first_async_query_timer_.has_value()
                              ? first_async_query_timer_->Elapsed()
                              : base::TimeDelta());

  base::UmaHistogramCounts10000(
      "Cookie.FirstPartySets.Network.DelayedQueriesCount",
      pending_queries_ ? pending_queries_->size() : 0);

  if (!pending_queries_) {
    return;
  }

  std::unique_ptr<base::circular_deque<base::OnceClosure>> queue;
  queue.swap(pending_queries_);
  while (!queue->empty()) {
    base::OnceClosure query_task = std::move(queue->front());
    queue->pop_front();
    std::move(query_task).Run();
  }
}

void FirstPartySetsManager::SetCompleteSets(net::GlobalFirstPartySets sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sets_.has_value())
    return;
  sets_ = std::move(sets);
  InvokePendingQueries();
}

void FirstPartySetsManager::EnqueuePendingQuery(base::OnceClosure run_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!sets_.has_value());
  CHECK(pending_queries_);

  if (!first_async_query_timer_.has_value())
    first_async_query_timer_ = {base::ElapsedTimer()};

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network
