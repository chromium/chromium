// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// FirstPartySetsLoader loads information about First-Party Sets (specified
// here: https://github.com/privacycg/first-party-sets) into a members-to-owners
// map asynchronously and returns it with a callback. It requires input sources
// from the component updater via `SetComponentSets`, and the command line via
// `SetManuallySpecifiedSet`.
class FirstPartySetsLoader {
 public:
  using LoadCompleteOnceCallback = base::OnceCallback<void(
      base::flat_map<net::SchemefulSite, net::SchemefulSite>)>;

  explicit FirstPartySetsLoader(LoadCompleteOnceCallback on_load_complete);
  ~FirstPartySetsLoader();

  FirstPartySetsLoader(const FirstPartySetsLoader&) = delete;
  FirstPartySetsLoader& operator=(const FirstPartySetsLoader&) = delete;

  // Stores the First-Party Set that was provided via the `kUseFirstPartySet`
  // flag/switch.
  void SetManuallySpecifiedSet(const std::string& flag_value);

  // Asynchronously parses and stores the sets from `sets_file` into the
  // members-to-owners map `sets_`, and merges with any previously-loaded sets
  // as needed. In case of invalid input, the set of sets provided by the file
  // is considered empty.
  //
  // Only the first call to SetComponentSets can have any effect; subsequent
  // invocations are ignored.
  void SetComponentSets(base::File sets_file);

  // Close the file on thread pool that allows blocking.
  void DisposeFile(base::File sets_file);

 private:
  // Parses the contents of `raw_sets` as a collection of First-Party Set
  // declarations, and assigns to `sets_`.
  void OnReadSetsFile(const std::string& raw_sets);

  // We must ensure there's no intersection between the manually-specified set
  // and the sets that came from Component Updater. (When reconciling the
  // manually-specified set and `sets_`, entries in the manually-specified set
  // always win.) We must also ensure that `sets_` includes the set described by
  // `manually_specified_set_`.
  void ApplyManuallySpecifiedSet();

  // Checks the required inputs have been received, and if so, invokes the
  // callback `on_load_complete_`.
  void MaybeFinishLoading();

  // Represents the mapping of site -> site, where keys are members of sets,
  // and values are owners of the sets (explicitly including an entry of owner
  // -> owner).
  // It holds partial data until all of the sources (component updater +
  // manually specified) have been merged, and then holds the merged data.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> sets_
      GUARDED_BY_CONTEXT(sequence_checker_);
  absl::optional<
      std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>>
      manually_specified_set_ GUARDED_BY_CONTEXT(sequence_checker_);

  enum Progress {
    kNotStarted,
    kStarted,
    kFinished,
  };

  Progress component_sets_parse_progress_
      GUARDED_BY_CONTEXT(sequence_checker_) = kNotStarted;
  bool manual_sets_ready_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // We use a OnceCallback to ensure we only pass along the completed sets once.
  LoadCompleteOnceCallback on_load_complete_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsLoader> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
