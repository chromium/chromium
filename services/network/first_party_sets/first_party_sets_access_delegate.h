// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom-forward.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace net {
class FirstPartySetMetadata;
class FirstPartySetsContextConfig;
class SchemefulSite;
}  // namespace net

namespace network {

class FirstPartySetsAccessDelegate
    : public mojom::FirstPartySetsAccessDelegate {
 public:
  using EntriesResult = FirstPartySetsManager::EntriesResult;

  // Construct a FirstPartySetsAccessDelegate that provides customizations
  // and serves mojo requests for the underlying First-Party Sets info.
  // `*manager` outlives this object.
  FirstPartySetsAccessDelegate(
      mojo::PendingReceiver<mojom::FirstPartySetsAccessDelegate> receiver,
      mojom::FirstPartySetsAccessDelegateParamsPtr params,
      FirstPartySetsManager* const manager);

  FirstPartySetsAccessDelegate(const FirstPartySetsAccessDelegate&) = delete;
  FirstPartySetsAccessDelegate& operator=(const FirstPartySetsAccessDelegate&) =
      delete;

  ~FirstPartySetsAccessDelegate() override;

  // mojom::FirstPartySetsAccessDelegate
  void NotifyReady(mojom::FirstPartySetsReadyEventPtr ready_event) override;
  void SetEnabled(bool enabled) override;

  // Computes the First-Party Set metadata and cache filter match info related
  // to the given context.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] std::optional<
      std::pair<net::FirstPartySetMetadata,
                net::FirstPartySetsCacheFilter::MatchInfo>>
  ComputeMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata,
                              net::FirstPartySetsCacheFilter::MatchInfo)>
          callback);

  // Calls FirstPartySetsManager::FindEntries either asynchronously or
  // synchronously, once initialization is complete.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] std::optional<EntriesResult> FindEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(EntriesResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const std::optional<net::SchemefulSite> top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata,
                              net::FirstPartySetsCacheFilter::MatchInfo)>
          callback) const;

  // Same as `FindEntries`, but plumbs the result into the callback. Must only
  // be called once the instance is fully initialized.
  void FindEntriesAndInvoke(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(EntriesResult)> callback) const;

  // Runs all pending queries. Must not be called until the instance is fully
  // initialized.
  void InvokePendingQueries();

  // Enqueues a query to be answered once the instance is fully initialized.
  void EnqueuePendingQuery(base::OnceClosure run_query);

  net::FirstPartySetsContextConfig* context_config() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ready_event_.has_value() ? &(ready_event_.value()->config) : nullptr;
  }

  net::FirstPartySetsCacheFilter* cache_filter() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ready_event_.has_value() ? &(ready_event_.value()->cache_filter)
                                    : nullptr;
  }

  // The underlying FirstPartySetsManager instance, which lives on the network
  // service.
  const raw_ptr<FirstPartySetsManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether First-Party Sets is enabled for this context in particular. Note
  // that this is unrelated to `manager_.is_enabled`. This may be reassigned via
  // `SetEnabled`.
  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether this instance should wait for First-Party Sets initialization (in
  // the browser process) before responding to queries.
  const bool wait_for_init_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The first ReadyEvent received. This is set at most once, and is immutable
  // thereafter.
  std::optional<mojom::FirstPartySetsReadyEventPtr> ready_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of queries that are waiting for the instance to be initialized.
  // This is non-null exactly when `ready_event_` is nullopt.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<mojom::FirstPartySetsAccessDelegate> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Timer starting when the first async query was enqueued, if any. Used for
  // metrics.
  std::optional<base::ElapsedTimer> first_async_query_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
