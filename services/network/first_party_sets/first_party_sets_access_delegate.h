// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class FirstPartySetMetadata;
class SchemefulSite;
}  // namespace net

namespace network {

class FirstPartySetsAccessDelegate
    : public mojom::FirstPartySetsAccessDelegate {
 public:
  using EntriesResult = FirstPartySetsManager::EntriesResult;
  using FlattenedSets = FirstPartySetsManager::FlattenedSets;

  // Construct a FirstPartySetsAccessDelegate that provides customizations
  // and serves mojo requests for the underlying First-Party Sets info.
  // `*manager` outlives this object.
  explicit FirstPartySetsAccessDelegate(
      mojo::PendingReceiver<mojom::FirstPartySetsAccessDelegate> receiver,
      mojom::FirstPartySetsAccessDelegateParamsPtr params,
      FirstPartySetsManager* const manager);

  FirstPartySetsAccessDelegate(const FirstPartySetsAccessDelegate&) = delete;
  FirstPartySetsAccessDelegate& operator=(const FirstPartySetsAccessDelegate&) =
      delete;

  ~FirstPartySetsAccessDelegate() override;

  // mojom::FirstPartySetsAccessDelegate
  void NotifyReady(mojom::FirstPartySetsReadyEventPtr ready_event) override;

  bool is_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return enabled_ && manager_->is_enabled();
  }

  // Computes the First-Party Set metadata related to the given context.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<net::FirstPartySetMetadata> ComputeMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

  // Calls FirstPartySetsManager::FindEntries either asynchronously or
  // synchronously, once initialization is complete.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<EntriesResult> FindEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(EntriesResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const absl::optional<net::SchemefulSite> top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const;

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

  // The underlying FirstPartySetsManager instance, which lives on the network
  // service.
  const raw_ptr<FirstPartySetsManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether First-Party Sets is enabled for this context in particular. Note
  // that this is unrelated to `manager_.is_enabled`.
  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // First-Party Sets configuration for this network context.
  net::FirstPartySetsContextConfig context_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of queries that are waiting for the instance to be initialized.
  // This is only set if we haven't been notified that we're ready to answer
  // queries.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<mojom::FirstPartySetsAccessDelegate> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Timer starting when the first async query was enqueued, if any. Used for
  // metrics.
  absl::optional<base::ElapsedTimer> first_async_query_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_