// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "services/network/first_party_sets/first_party_sets_context_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// Class FirstPartySetsManager is a pseudo-singleton owned by NetworkService; it
// answers queries about First-Party Sets after they've been loaded.
class FirstPartySetsManager {
 public:
  using SetsByOwner =
      base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>;
  using OwnerResult = absl::optional<net::SchemefulSite>;
  using OwnersResult = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using FlattenedSets = base::flat_map<net::SchemefulSite, net::SchemefulSite>;

  explicit FirstPartySetsManager(bool enabled);
  ~FirstPartySetsManager();

  FirstPartySetsManager(const FirstPartySetsManager&) = delete;
  FirstPartySetsManager& operator=(const FirstPartySetsManager&) = delete;

  bool is_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return enabled_;
  }

  // Computes the First-Party Set metadata related to the given request context.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<net::FirstPartySetMetadata> ComputeMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

  // Computes a mapping from owner to set members. For convenience of iteration,
  // the members of the set includes the owner.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<SetsByOwner> Sets(
      const FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(SetsByOwner)> callback);

  // Stores the First-Party Sets data.
  //
  // Only the first call to SetCompleteSets can have any effect; subsequent
  // invocations are ignored.
  void SetCompleteSets(FlattenedSets sets);

  // Sets the enabled_ attribute for testing.
  void SetEnabledForTesting(bool enabled);

  // Returns nullopt if First-Party Sets is disabled or if the input is not in
  // a nontrivial set.
  // If FPS is enabled and the input site is in a nontrivial set, then this
  // returns the owner site of that set.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<OwnerResult> FindOwner(
      const net::SchemefulSite& site,
      const FirstPartySetsContextConfig& fps_context_config,
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
      const FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(OwnersResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const absl::optional<net::SchemefulSite> top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback,
      base::TimeTicks enqueued_at) const;

  // Synchronous version of `ComputeMetadata`, to be run only once the instance
  // is fully initialized.
  net::FirstPartySetMetadata ComputeMetadataInternal(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Returns whether the `site` is same-party with the `party_context`, and
  // `top_frame_site` (if it is not nullptr). That is, is the `site`'s owner the
  // same as the owners of every member of `party_context` and of
  // `top_frame_site`? Note: if `site` is not a member of a First-Party Set
  // (with more than one member), then this returns false. If `top_frame_site`
  // is nullptr, then it is ignored.
  bool IsContextSamePartyWithSite(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config,
      bool infer_singleton_sets) const;

  // Computes the "type" of the context. I.e., categorizes contexts based on
  // whether the top frame site and resource URL are same-party; whether the top
  // frame site was ignored; whether the `party_context` is same-party with
  // everything else; etc.
  //
  // Since this metric may be used to inform decisions based on actual usage
  // patterns of sites on the web, this infers singleton sets. That is, it
  // treats sites that do not belong to a First-Party Set as belonging to an
  // implictly-declared singleton First-Party Set.
  net::FirstPartySetsContextType ComputeContextType(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Same as `FindOwner`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void FindOwnerAndInvoke(const net::SchemefulSite& site,
                          const FirstPartySetsContextConfig& fps_context_config,
                          base::OnceCallback<void(OwnerResult)> callback,
                          base::TimeTicks enqueued_at) const;

  // Returns `site`'s owner (optionally inferring a singleton set if necessary),
  // or `nullopt` if `site` has no owner. Must not return `nullopt` if
  // `infer_singleton_sets` is true. `fps_context_config` is the configuration
  // to be used in this context.
  //
  // This is synchronous, and must not be called
  // until the instance is fully initialized.
  const absl::optional<net::SchemefulSite> FindOwnerInternal(
      const net::SchemefulSite& site,
      const FirstPartySetsContextConfig& fps_context_config,
      bool infer_singleton_sets) const;

  // Same as `FindOwners`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void FindOwnersAndInvoke(
      const base::flat_set<net::SchemefulSite>& sites,
      const FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(OwnersResult)> callback,
      base::TimeTicks enqueued_at) const;

  // Synchronous version of `FindOwners`, to be run only once the instance is
  // initialized.
  OwnersResult FindOwnersInternal(
      const base::flat_set<net::SchemefulSite>& sites,
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Same as `Sets`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void SetsAndInvoke(const FirstPartySetsContextConfig& fps_context_config,
                     base::OnceCallback<void(SetsByOwner)> callback,
                     base::TimeTicks enqueued_at) const;

  // Synchronous version of `Sets`, to be run only once the instance is
  // initialized.
  SetsByOwner SetsInternal(
      const FirstPartySetsContextConfig& fps_context_config) const;

  // Enqueues a query to be answered once the instance is fully initialized.
  void EnqueuePendingQuery(base::OnceClosure run_query);

  // Runs all pending queries. Must not be called until the instance is fully
  // initialized.
  void InvokePendingQueries();

  // Represents the mapping of site -> site, where keys are members of sets, and
  // values are owners of the sets. Owners are explicitly represented as members
  // of the set.
  //
  // Optional because it is unset until all of the required inputs have been
  // received.
  absl::optional<FlattenedSets> sets_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The queue of queries that are waiting for the instance to be initialized.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The time when the first async query was enqueued, if any. Used for metrics.
  absl::optional<base::TimeTicks> first_async_query_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsManager> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(PopulatedFirstPartySetsManagerTest,
                           ComputeContextType);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
