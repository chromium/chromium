// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// FirstPartySetsLoader loads information about First-Party Sets (specified
// here: https://github.com/privacycg/first-party-sets) into a members-to-owners
// map asynchronously and returns it with a callback. It requires input sources
// from the component updater via `SetComponentSets`, and the command line via
// `SetManuallySpecifiedSet`.
class CONTENT_EXPORT FirstPartySetsLoader {
 public:
  using LoadCompleteOnceCallback = base::OnceCallback<void(
      base::flat_map<net::SchemefulSite, net::SchemefulSite>)>;
  using FlattenedSets = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using SingleSet =
      std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>;

  FirstPartySetsLoader(LoadCompleteOnceCallback on_load_complete,
                       base::Value::Dict policy_overrides);

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

  // Modifies `sets_` to include the CLI-provided set, if any. Must not be
  // called until the loader has received the CLI flag value via
  // `SetManuallySpecifiedSet`, and the public sets via `SetComponentSets`.
  void ApplyManuallySpecifiedSet();

  // Checks the required inputs have been received, and if so, invokes the
  // callback `on_load_complete_`, after merging sets appropriately.
  void MaybeFinishLoading();

  // Represents the mapping of site -> site, where keys are members of sets,
  // and values are owners of the sets (explicitly including an entry of owner
  // -> owner).
  // It holds partial data until all of the sources (component updater +
  // manually specified) have been merged, and then holds the merged data.
  FlattenedSets sets_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the set that was provided on the command line (if any). There are two
  // layers of absl::optional here because the value is initially unset (outer
  // optional), and may be empty if no command-line flag was provided (or one
  // was provided but invalid) (inner optional).
  absl::optional<absl::optional<SingleSet>> manually_specified_set_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Contains two (possibly empty) lists of SingleSets which are provided to
  // override |sets_| either by replacement or addition. The type of override
  // that a SingleSet should be used for is specified by the
  // ParsedPolicySetLists field.
  //
  // This variable is not set by any methods other than the constructor.
  FirstPartySetParser::ParsedPolicySetLists policy_overrides_
      GUARDED_BY_CONTEXT(sequence_checker_);

  enum Progress {
    kNotStarted,
    kStarted,
    kFinished,
  };

  Progress component_sets_parse_progress_
      GUARDED_BY_CONTEXT(sequence_checker_) = kNotStarted;

  // We use a OnceCallback to ensure we only pass along the completed sets once.
  LoadCompleteOnceCallback on_load_complete_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for latency metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
