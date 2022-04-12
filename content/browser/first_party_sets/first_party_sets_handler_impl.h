// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Class FirstPartySetsHandlerImpl is a singleton, it allows an embedder to
// provide First-Party Sets inputs from custom sources, then parses/merges the
// inputs to form the current First-Party Sets data, compares them with the
// persisted First-Party Sets data used during the last browser session to get a
// list of sites that changed the First-Party Set they are part of, invokes the
// provided callback with the current First-Party Sets data, and writes
// the current First-Party Sets data to disk.
class CONTENT_EXPORT FirstPartySetsHandlerImpl : public FirstPartySetsHandler {
 public:
  using FlattenedSets = base::flat_map<net::SchemefulSite, net::SchemefulSite>;
  using SetsReadyOnceCallback = base::OnceCallback<void(const FlattenedSets&)>;

  static FirstPartySetsHandlerImpl* GetInstance();

  ~FirstPartySetsHandlerImpl() override;

  FirstPartySetsHandlerImpl(const FirstPartySetsHandlerImpl&) = delete;
  FirstPartySetsHandlerImpl& operator=(const FirstPartySetsHandlerImpl&) =
      delete;

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir`, sets the First-Party Set that was provided via the
  // flag/switch, and sets a callback that should eventually be invoked with the
  // current First-Party Sets data.
  //
  // If First-Party Sets is disabled, then this method still needs to read the
  // persisted sets, since we may still need to clear data from a previous
  // invocation of Chromium which had First-Party Sets enabled.
  //
  // TODO(https://crbug.com/1309188): Init() should be called in the
  // BrowserMainLoop::PreMainMessageLoopRun(). But just in case it's
  // accidentally called from other places, make sure it's no-op for the
  // following calls.
  void Init(const base::FilePath& user_data_dir,
            const std::string& flag_value,
            SetsReadyOnceCallback on_sets_ready);

  // Returns the current First-Party Sets data, if the data is ready and the
  // feature is enabled.
  absl::optional<FlattenedSets> GetSetsIfEnabledAndReady() const;

  // FirstPartySetsHandler
  bool IsEnabled() const override;
  void SetPublicFirstPartySets(base::File sets_file) override;
  void ResetForTesting() override;

  // Sets whether FPS is enabled (for testing).
  void SetEnabledForTesting(bool enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    enabled_ = enabled;
  }

  // Compares the map `old_sets` to `current_sets` and returns the set of sites
  // that: 1) were in `old_sets` but are no longer in `current_sets`, i.e. leave
  // the FPSs; or, 2) mapped to a different owner site.
  //
  // This method assumes that the sites were normalized properly when the maps
  // were created. Made public only for testing,
  static base::flat_set<net::SchemefulSite> ComputeSetsDiff(
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>& old_sets,
      const base::flat_map<net::SchemefulSite, net::SchemefulSite>&
          current_sets);

 private:
  friend class base::NoDestructor<FirstPartySetsHandlerImpl>;

  explicit FirstPartySetsHandlerImpl(bool enabled);

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir`.
  void SetPersistedSets(const base::FilePath& user_data_dir);

  // Stores the read persisted sets in `raw_persisted_sets_`.
  void OnReadPersistedSetsFile(const std::string& raw_persisted_sets);

  // Parses and sets the First-Party Set that was provided via the
  // `kUseFirstPartySet` flag/switch.
  //
  // Has no effect if `kFirstPartySets` is disabled, or
  // `SetPublicFirstPartySets` is not called.
  void SetManuallySpecifiedSet(const std::string& flag_value);

  // Sets the current First-Party Sets data.
  void SetCompleteSets(FlattenedSets sets);

  // Checks the required inputs have been received, and if so:
  // 1) computes the diff between the `sets_` and the parsed
  // `raw_persisted_sets_`;
  // 2) clears the site data of the set of sites based on the diff;
  // 3) calls `on_sets_ready_` if conditions are met;
  // 4) writes the current First-Party Sets to the file in
  // `persisted_sets_path_`.
  //
  // TODO(shuuran@chromium.org): Implement the code to clear site state.
  void ClearSiteDataOnChangedSetsIfReady();

  // Returns true if:
  // * First-Party Sets are enabled;
  // * `sets_` is ready to be used.
  bool IsEnabledAndReady() const;

  // Represents the mapping of site -> site, where keys are members of sets, and
  // values are owners of the sets. Owners are explicitly represented as members
  // of the set.
  //
  // Optional because it is unset until all of the required inputs have been
  // received.
  absl::optional<FlattenedSets> sets_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The sets that were persisted during the last run of Chrome. Initially unset
  // (nullopt) until it has been read from disk.
  absl::optional<std::string> raw_persisted_sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The path where persisted First-Party sets data is stored.
  base::FilePath persisted_sets_path_ GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_);

  // We use a OnceCallback to ensure we only pass along the sets once
  // during Chrome's lifetime (modulo reconfiguring the network service).
  SetsReadyOnceCallback on_sets_ready_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<FirstPartySetsLoader> sets_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
