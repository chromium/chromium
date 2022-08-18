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
#include "net/cookies/first_party_set_entry.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/first_party_sets_context_config.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// Class FirstPartySetsManager is a pseudo-singleton owned by NetworkService; it
// answers queries about First-Party Sets after they've been loaded.
class FirstPartySetsManager {
 public:
  using OwnersResult =
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>;
  using FlattenedSets =
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>;

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
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

  // Stores the First-Party Sets data.
  //
  // Only the first call to SetCompleteSets can have any effect; subsequent
  // invocations are ignored.
  void SetCompleteSets(mojom::PublicFirstPartySetsPtr public_sets);

  // Sets the enabled_ attribute for testing.
  void SetEnabledForTesting(bool enabled);

  // Returns the mapping of sites to entries for the given input sites (if an
  // entry exists).
  //
  // When FPS is disabled, returns an empty map.
  // When FPS is enabled, this maps each input site to its entry (if one
  // exists), and returns the resulting mapping. If a site isn't in a
  // non-trivial First-Party Set, it is not added to the output map.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] absl::optional<OwnersResult> FindOwners(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(OwnersResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const absl::optional<net::SchemefulSite> top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback,
      base::ElapsedTimer timer) const;

  // Synchronous version of `ComputeMetadata`, to be run only once the instance
  // is fully initialized.
  net::FirstPartySetMetadata ComputeMetadataInternal(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

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
      const net::FirstPartySetsContextConfig& fps_context_config) const;

  // Returns `site`'s entry, or `nullopt` if `site` has no entry.
  // `fps_context_config` is the configuration to be used in this context.
  //
  // This is synchronous, and must not be called until the instance is fully
  // initialized.
  absl::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

  // Same as `FindOwners`, but plumbs the result into the callback. Must only be
  // called once the instance is fully initialized.
  void FindOwnersAndInvoke(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(OwnersResult)> callback,
      base::ElapsedTimer timer) const;

  // Synchronous version of `FindOwners`, to be run only once the instance is
  // initialized.
  OwnersResult FindOwnersInternal(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

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

  // The site aliases. Used to normalize a given SchemefulSite into its
  // canonical representative, before looking it up in `sets_`.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> aliases_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The queue of queries that are waiting for the instance to be initialized.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the first async query was enqueued, if any. Used for
  // metrics.
  absl::optional<base::ElapsedTimer> first_async_query_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
