// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_LOADER_H_

#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "content/common/content_export.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"

namespace content {

// FirstPartySetsLoader loads information about First-Party Sets (specified
// here: https://github.com/privacycg/first-party-sets) into a
// members-to-primaries map asynchronously and returns it with a callback. It
// requires input sources from the component updater via `SetComponentSets`, and
// the command line via `SetManuallySpecifiedSet`.
class CONTENT_EXPORT FirstPartySetsLoader {
 public:
  using LoadCompleteOnceCallback =
      base::OnceCallback<void(net::GlobalFirstPartySets)>;

  explicit FirstPartySetsLoader(LoadCompleteOnceCallback on_load_complete);

  ~FirstPartySetsLoader();

  FirstPartySetsLoader(const FirstPartySetsLoader&) = delete;
  FirstPartySetsLoader& operator=(const FirstPartySetsLoader&) = delete;

  // Stores the First-Party Set that was provided via the `kUseFirstPartySet`
  // flag/switch. Only the first call has any effect.
  void SetManuallySpecifiedSet(const net::LocalSetDeclaration& local_set);

  // Asynchronously parses and stores the sets from `sets_file`, and merges with
  // any previously-loaded sets as needed. In case of invalid input, the set of
  // sets provided by the file is considered empty.
  //
  // Only the first call to SetComponentSets can have any effect; subsequent
  // invocations are ignored.
  void SetComponentSets(base::Version version, base::File sets_file);

  // Closes the given file safely.
  static void DisposeFile(base::File file);

 private:
  // Parses the contents of `raw_sets` as a collection of First-Party Set
  // declarations, and stores the result.
  void OnReadSetsFile(base::Version version, const std::string& raw_sets);

  // Checks the required inputs have been received, and if so, invokes the
  // callback `on_load_complete_`, after merging sets appropriately.
  void MaybeFinishLoading();

  // Holds the global First-Party Sets. This is nullopt until received from
  // Component Updater. It may be modified based on the manually-specified set.
  std::optional<net::GlobalFirstPartySets> sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Holds the set that was provided on the command line (if any). This is
  // nullopt until `SetManuallySpecifiedSet` is called.
  std::optional<net::LocalSetDeclaration> manually_specified_set_
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
