// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
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
      wait_for_init_(base::FeatureList::IsEnabled(
          net::features::kWaitForFirstPartySetsInit)),
      ready_event_(receiver.is_valid() && manager->is_enabled()
                       ? std::nullopt
                       : std::make_optional(
                             network::mojom::FirstPartySetsReadyEvent::New())),
      pending_queries_(
          ready_event_.has_value() || !wait_for_init_
              ? nullptr
              : std::make_unique<base::circular_deque<base::OnceClosure>>()) {
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
  InvokePendingQueries();
}

// TODO(crbug.com/1366846): Add metrics to track whether this is called from
// dynamic policy updates before NotifyReady.
void FirstPartySetsAccessDelegate::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  enabled_ = enabled;
}

std::optional<std::pair<net::FirstPartySetMetadata,
                        net::FirstPartySetsCacheFilter::MatchInfo>>
FirstPartySetsAccessDelegate::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata,
                            net::FirstPartySetsCacheFilter::MatchInfo)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_) {
    return std::make_pair(net::FirstPartySetMetadata(),
                          net::FirstPartySetsCacheFilter::MatchInfo());
  }
  if (!ready_event_.has_value()) {
    if (!wait_for_init_) {
      return std::make_pair(net::FirstPartySetMetadata(),
                            net::FirstPartySetsCacheFilter::MatchInfo());
    }
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsAccessDelegate::ComputeMetadataAndInvoke,
        base::Unretained(this), site, base::OptionalFromPtr(top_frame_site),
        std::move(callback)));
    return std::nullopt;
  }

  net::FirstPartySetsCacheFilter::MatchInfo match_info(
      cache_filter()->GetMatchInfo(site));

  std::optional<net::FirstPartySetMetadata> metadata =
      manager_->ComputeMetadata(
          site, top_frame_site, *context_config(),
          base::BindOnce(
              [](base::OnceCallback<void(
                     net::FirstPartySetMetadata,
                     net::FirstPartySetsCacheFilter::MatchInfo)> callback,
                 net::FirstPartySetsCacheFilter::MatchInfo info,
                 net::FirstPartySetMetadata metadata) {
                std::move(callback).Run(std::move(metadata), std::move(info));
              },
              std::move(callback), match_info));

  return metadata.has_value() ? std::make_optional(std::make_pair(
                                    std::move(metadata).value(), match_info))
                              : std::nullopt;
}

std::optional<FirstPartySetsAccessDelegate::EntriesResult>
FirstPartySetsAccessDelegate::FindEntries(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled_)
    return FirstPartySetsAccessDelegate::EntriesResult();

  if (!ready_event_.has_value()) {
    if (!wait_for_init_) {
      return FirstPartySetsAccessDelegate::EntriesResult();
    }
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsAccessDelegate::FindEntriesAndInvoke,
                       base::Unretained(this), sites, std::move(callback)));
    return std::nullopt;
  }

  return manager_->FindEntries(sites, *context_config(), std::move(callback));
}

void FirstPartySetsAccessDelegate::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const std::optional<net::SchemefulSite> top_frame_site,
    base::OnceCallback<void(net::FirstPartySetMetadata,
                            net::FirstPartySetsCacheFilter::MatchInfo)>
        callback) const {
  using CallbackType = decltype(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(context_config());
  // NB: since `ComputeMetadata` returns early if the delegate is disabled,
  // we're guaranteed that for any queued query, the delegate must have been
  // enabled when the query was received. However, the delegate may have been
  // disabled between then and now, so we have no guarantees re: `enabled_` now.

  std::pair<CallbackType, CallbackType> callbacks =
      base::SplitOnceCallback(std::move(callback));

  net::FirstPartySetsCacheFilter::MatchInfo match_info(
      cache_filter()->GetMatchInfo(site));

  std::optional<net::FirstPartySetMetadata> sync_result =
      manager_->ComputeMetadata(
          site, base::OptionalToPtr(top_frame_site), *context_config(),
          base::BindOnce(
              [](CallbackType callback,
                 net::FirstPartySetsCacheFilter::MatchInfo match_info,
                 net::FirstPartySetMetadata metadata) {
                std::move(callback).Run(std::move(metadata), match_info);
              },
              std::move(callbacks.first), match_info));

  if (sync_result.has_value()) {
    std::move(callbacks.second).Run(std::move(sync_result.value()), match_info);
  }
}

void FirstPartySetsAccessDelegate::FindEntriesAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(context_config());
  // NB: since `FindEntries` returns early if the delegate is disabled,
  // we're guaranteed that for any queued query, the delegate must have been
  // enabled when the query was received. However, the delegate may have been
  // disabled between then and now, so we have no guarantees re: `enabled_` now.

  std::pair<
      base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>,
      base::OnceCallback<void(FirstPartySetsAccessDelegate::EntriesResult)>>
      callbacks = base::SplitOnceCallback(std::move(callback));

  std::optional<FirstPartySetsAccessDelegate::EntriesResult> sync_result =
      manager_->FindEntries(sites, *context_config(),
                            std::move(callbacks.first));

  if (sync_result.has_value())
    std::move(callbacks.second).Run(sync_result.value());
}

void FirstPartySetsAccessDelegate::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ready_event_.has_value());
  // !wait_for_init_ implies (pending_queries == nullptr).
  CHECK(wait_for_init_ || pending_queries_ == nullptr);
  // !wait_for_init_ implies !first_async_query_timer_.has_value().
  CHECK(wait_for_init_ || !first_async_query_timer_.has_value());

  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration."
      "ContextReadyToServeQueries2",
      construction_timer_.Elapsed());

  base::UmaHistogramCounts10000(
      "Cookie.FirstPartySets.ContextDelayedQueriesCount",
      pending_queries_ ? pending_queries_->size() : 0);

  base::UmaHistogramTimes("Cookie.FirstPartySets.ContextMostDelayedQueryDelta",
                          first_async_query_timer_.has_value()
                              ? first_async_query_timer_->Elapsed()
                              : base::TimeDelta());

  if (!pending_queries_) {
    return;
  }

  std::unique_ptr<base::circular_deque<base::OnceClosure>> queries;
  queries.swap(pending_queries_);
  while (!queries->empty()) {
    base::OnceClosure query_task = std::move(queries->front());
    queries->pop_front();
    std::move(query_task).Run();
  }
}

void FirstPartySetsAccessDelegate::EnqueuePendingQuery(
    base::OnceClosure run_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pending_queries_);
  CHECK(!ready_event_.has_value());
  CHECK(wait_for_init_);

  if (!first_async_query_timer_.has_value())
    first_async_query_timer_ = {base::ElapsedTimer()};

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network
