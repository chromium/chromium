// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/first_party_sets/first_party_sets_context_config.h"
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
  using SetsByOwner =
      base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>;
  using OwnerResult = absl::optional<net::SchemefulSite>;
  using OwnersResult = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using FlattenedSets = base::flat_map<net::SchemefulSite, net::SchemefulSite>;

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
    return context_config_.is_enabled() && manager_->is_enabled();
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

  // Computes a mapping from owner to set members. For convenience of iteration,
  // the members of the set includes the owner.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<SetsByOwner> Sets(
      base::OnceCallback<void(SetsByOwner)> callback);

  // Returns optional(nullopt) if First-Party Sets is disabled or if the input
  // is not in a nontrivial set.
  // If FPS is enabled and the input site is in a nontrivial set, then this
  // returns a non-empty optional containing the owner site of that set.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  //
  // Note that there is a semantic difference between optional(nullopt) and
  // nullopt.
  [[nodiscard]] absl::optional<OwnerResult> FindOwner(
      const net::SchemefulSite& site,
      base::OnceCallback<void(OwnerResult)> callback);

  // Batched version of `FindOwner`. Returns the mapping of sites to owners for
  // the given input sites (if an owner exists).
  //
  // When FPS is disabled, returns an empty map.
  // When FPS is enabled, this maps each input site to its owner (if one
  // exists), and returns the resulting mapping. If a site isn't in a
  // non-trivial First-Party Set, it is not added to the output map.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<OwnersResult> FindOwners(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(OwnersResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const absl::optional<net::SchemefulSite> top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const;

  // Same as `Sets`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void SetsAndInvoke(base::OnceCallback<void(SetsByOwner)> callback) const;

  // Same as `FindOwner`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void FindOwnerAndInvoke(const net::SchemefulSite& site,
                          base::OnceCallback<void(OwnerResult)> callback) const;

  // Same as `FindOwners`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void FindOwnersAndInvoke(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<void(OwnersResult)> callback) const;

  // Runs all pending queries. Must not be called until the instance is fully
  // initialized.
  void InvokePendingQueries();

  // Enqueues a query to be answered once the instance is fully initialized.
  void EnqueuePendingQuery(base::OnceClosure run_query);

  // The underlying FirstPartySetsManager instance, which lives on the network
  // service.
  const raw_ptr<FirstPartySetsManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // First-Party Sets configuration for this network context.
  FirstPartySetsContextConfig context_config_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of queries that are waiting for the instance to be initialized.
  // This is only set if we haven't been notified that we're ready to answer
  // queries.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<mojom::FirstPartySetsAccessDelegate> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_