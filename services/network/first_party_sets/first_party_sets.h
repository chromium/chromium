// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// Class FirstPartySets is a pseudo-singleton owned by NetworkService; it stores
// all known information about First-Party Sets state. This information is
// updated by the component updater via |ParseAndSet|.
class FirstPartySets {
 public:
  explicit FirstPartySets(bool enabled);
  ~FirstPartySets();

  FirstPartySets(const FirstPartySets&) = delete;
  FirstPartySets& operator=(const FirstPartySets&) = delete;

  bool is_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return enabled_;
  }

  // Stores the First-Party Set that was provided via the `kUseFirstPartySet`
  // flag/switch.
  //
  // Has no effect if `kFirstPartySets` is disabled.
  void SetManuallySpecifiedSet(const std::string& flag_value);

  // Asynchronously overwrites the current members-to-owners map with the sets
  // in `sets_file`.  `sets_file` is expected to contain either a JSON-encoded
  // array of records, or a sequence of newline-delimited JSON records. Each
  // record is a set declaration in the format specified here:
  // https://github.com/privacycg/first-party-sets.
  //
  // Only the first call to ParseAndSet can have any effect; subsequent
  // invocations are ignored.
  //
  // If `sets_file.IsValid()` is false, then the set of sets is considered
  // empty.
  //
  // In case of invalid input, the set of sets provided by the file is
  // considered empty. Note that the FirstPartySets instance may still have some
  // sets, from the command line or enterprise policies.
  //
  // Has no effect if `kFirstPartySets` is disabled.
  void ParseAndSet(base::File sets_file);

  // Computes the First-Party Set metadata related to the given context.
  net::FirstPartySetMetadata ComputeMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context) const;

  int64_t size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return sets_.size();
  }

  // Returns a mapping from owner to set members. For convenience of iteration,
  // the members of the set includes the owner.
  base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>> Sets() const;

  // Sets the `raw_persisted_sets_`, which is a JSON-encoded
  // string representation of a map of site -> site.
  void SetPersistedSets(base::StringPiece persisted_sets);
  // Sets the `on_site_data_cleared_` callback, which takes input of a
  // JSON-encoded string representation of a map of site -> site.
  void SetOnSiteDataCleared(
      base::OnceCallback<void(const std::string&)> callback);
  // Sets the enabled_ attribute for testing.
  void SetEnabledForTesting(bool enabled);

  // Returns nullopt if First-Party Sets are disabled or if the input is not in
  // a nontrivial set.
  // If FPS are enabled and the input site is in a nontrivial set, then this
  // returns the owner site of that set.
  const absl::optional<net::SchemefulSite> FindOwner(
      const net::SchemefulSite& site) const;

 private:
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
      const std::set<net::SchemefulSite>& party_context) const;

  // Parses the contents of `raw_sets` as a collection of First-Party Set
  // declarations, and assigns to `sets_`.
  void OnReadSetsFile(const std::string& raw_sets);

  // Returns `site`'s owner (optionally inferring a singleton set if necessary),
  // or `nullopt` if `site` has no owner. Must not return `nullopt` if
  // `infer_singleton_sets` is true.
  const absl::optional<net::SchemefulSite> FindOwner(
      const net::SchemefulSite& site,
      bool infer_singleton_sets) const;

  // We must ensure there's no intersection between the manually-specified set
  // and the sets that came from Component Updater. (When reconciling the
  // manually-specified set and `sets_`, entries in the manually-specified set
  // always win.) We must also ensure that `sets_` includes the set described by
  // `manually_specified_set_`.
  void ApplyManuallySpecifiedSet();

  // Compares the map `old_sets` to `sets_` and returns the set of sites that:
  // 1) were in `old_sets` but are no longer in `sets_`, i.e. leave the FPSs;
  // or, 2) mapped to a different owner site.
  base::flat_set<net::SchemefulSite> ComputeSetsDiff(
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& old_sets)
      const;

  // Checks the required inputs have been received, and if so, computes the diff
  // between the `sets_` and the parsed `raw_persisted_sets_`, and clears the
  // site data of the set of sites based on the diff.
  //
  // TODO(shuuran@chromium.org): Implement the code to clear site state.
  void ClearSiteDataOnChangedSetsIfReady();

  // Represents the mapping of site -> site, where keys are members of sets, and
  // values are owners of the sets. Owners are explicitly represented as members
  // of the set.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> sets_
      GUARDED_BY_CONTEXT(sequence_checker_);
  absl::optional<
      std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>>
      manually_specified_set_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::string raw_persisted_sets_ GUARDED_BY_CONTEXT(sequence_checker_);

  enum Progress {
    kNotStarted,
    kStarted,
    kFinished,
  };

  bool persisted_sets_ready_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  Progress component_sets_parse_progress_
      GUARDED_BY_CONTEXT(sequence_checker_) = kNotStarted;
  bool manual_sets_ready_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // The callback runs after the site state clearing is completed.
  base::OnceCallback<void(const std::string&)> on_site_data_cleared_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySets> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_SitesJoined);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_SitesLeft);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_OwnerChanged);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_OwnerLeft);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_OwnerMemberRotate);
  FRIEND_TEST_ALL_PREFIXES(FirstPartySetsEnabledTest,
                           ComputeSetsDiff_EmptySets);
  FRIEND_TEST_ALL_PREFIXES(PopulatedFirstPartySetsTest, ComputeContextType);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_H_
