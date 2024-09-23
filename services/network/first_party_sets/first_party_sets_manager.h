// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace network {

// Class FirstPartySetsManager is a pseudo-singleton owned by NetworkService; it
// answers queries about First-Party Sets after they've been loaded.
class FirstPartySetsManager {
 public:
  using EntriesResult =
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
  [[nodiscard]] std::optional<net::FirstPartySetMetadata> ComputeMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback);

  // Stores the First-Party Sets data.
  //
  // Only the first call to SetCompleteSets can have any effect; subsequent
  // invocations are ignored.
  void SetCompleteSets(net::GlobalFirstPartySets sets);

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
  [[nodiscard]] std::optional<EntriesResult> FindEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(EntriesResult)> callback);

 private:
  // Same as `ComputeMetadata`, but plumbs the result into the callback. Must
  // only be called once the instance is fully initialized.
  void ComputeMetadataAndInvoke(
      const net::SchemefulSite& site,
      const std::optional<net::SchemefulSite> top_frame_site,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const;

  // Synchronous version of `ComputeMetadata`, to be run only once the instance
  // is fully initialized.
  net::FirstPartySetMetadata ComputeMetadataInternal(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

  // Returns `site`'s entry, or `nullopt` if `site` has no entry.
  // `fps_context_config` is the configuration to be used in this context.
  //
  // This is synchronous, and must not be called until the instance is fully
  // initialized.
  std::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

  // Same as `FindEntries`, but plumbs the result into the callback. Must only
  // be called once the instance is fully initialized.
  void FindEntriesAndInvoke(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config,
      base::OnceCallback<void(EntriesResult)> callback) const;

  // Synchronous version of `FindEntries`, to be run only once the instance is
  // initialized.
  EntriesResult FindEntriesInternal(
      const base::flat_set<net::SchemefulSite>& sites,
      const net::FirstPartySetsContextConfig& fps_context_config) const;

  // Enqueues a query to be answered once the instance is fully initialized.
  void EnqueuePendingQuery(base::OnceClosure run_query);

  // Runs all pending queries. Must not be called until the instance is fully
  // initialized.
  void InvokePendingQueries();

  // The global First-Party Sets data.
  //
  // Optional because it is unset until the data has been received from the
  // browser process.
  std::optional<net::GlobalFirstPartySets> sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Whether this instance should wait for First-Party Sets initialization (in
  // the browser process) before responding to queries.
  const bool wait_for_init_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The queue of queries that are waiting for the instance to be initialized.
  std::unique_ptr<base::circular_deque<base::OnceClosure>> pending_queries_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the first async query was enqueued, if any. Used for
  // metrics.
  std::optional<base::ElapsedTimer> first_async_query_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
