// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_INSTANCE_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_INSTANCE_H_

#include <optional>
#include <string>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/common/content_export.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/first_party_sets/local_set_declaration.h"

namespace net {
class FirstPartySetEntry;
class SchemefulSite;
}  // namespace net

namespace content {

class BrowserContext;

// Class FirstPartySetsHandlerImplInstance is a singleton, it allows an embedder
// to provide First-Party Sets inputs from custom sources, then parses/merges
// the inputs to form the current First-Party Sets data, compares them with the
// persisted First-Party Sets data used during the last browser session to get
// a list of sites that changed the First-Party Set they are part of, invokes
// the provided callback with the current First-Party Sets data, and writes
// the current First-Party Sets data to disk.
class CONTENT_EXPORT FirstPartySetsHandlerImplInstance
    : public FirstPartySetsHandlerImpl {
 public:
  ~FirstPartySetsHandlerImplInstance() override;

  FirstPartySetsHandlerImplInstance(const FirstPartySetsHandlerImplInstance&) =
      delete;
  FirstPartySetsHandlerImplInstance& operator=(
      const FirstPartySetsHandlerImplInstance&) = delete;

  // Factory method that exposes the ctor for testing.
  static FirstPartySetsHandlerImplInstance CreateForTesting(
      bool enabled,
      bool embedder_will_provide_public_sets);

  // FirstPartySetsHandlerImpl:
  void Init(const base::FilePath& user_data_dir,
            const net::LocalSetDeclaration& local_set) override;
  [[nodiscard]] std::optional<net::GlobalFirstPartySets> GetSets(
      base::OnceCallback<void(net::GlobalFirstPartySets)> callback) override;

  // FirstPartySetsHandler:
  bool IsEnabled() const override;
  void SetPublicFirstPartySets(const base::Version& version,
                               base::File sets_file) override;
  std::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site,
      const net::FirstPartySetsContextConfig& config) const override;
  void GetContextConfigForPolicy(
      const base::Value::Dict* policy,
      base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback)
      override;
  void ClearSiteDataOnChangedSetsForContext(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config,
      base::OnceCallback<void(net::FirstPartySetsContextConfig,
                              net::FirstPartySetsCacheFilter)> callback)
      override;
  void ComputeFirstPartySetMetadata(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const net::FirstPartySetsContextConfig& config,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) override;
  bool ForEachEffectiveSetEntry(
      const net::FirstPartySetsContextConfig& config,
      base::FunctionRef<bool(const net::SchemefulSite&,
                             const net::FirstPartySetEntry&)> f) const override;
  void GetPersistedSetsForTesting(
      const std::string& browser_context_id,
      base::OnceCallback<
          void(std::optional<std::pair<net::GlobalFirstPartySets,
                                       net::FirstPartySetsContextConfig>>)>
          callback);
  void HasBrowserContextClearedForTesting(
      const std::string& browser_context_id,
      base::OnceCallback<void(std::optional<bool>)> callback);

  void SynchronouslyResetDBHelperForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    db_helper_.SynchronouslyResetForTest();  // IN-TEST
  }

 private:
  friend class base::NoDestructor<FirstPartySetsHandlerImplInstance>;

  FirstPartySetsHandlerImplInstance(bool enabled,
                                    bool embedder_will_provide_public_sets);

  // Sets the global First-Party Sets data. Must be called exactly once.
  void SetCompleteSets(net::GlobalFirstPartySets sets);

  // Sets `db_helper_`, which will initialize the underlying First-Party Sets
  // database under `user_data_dir`. Must be called exactly once.
  void SetDatabase(const base::FilePath& user_data_dir);

  // Enqueues a task to be performed once initialization is complete.
  void EnqueuePendingTask(base::OnceClosure run_task);

  // Invokes any pending queries.
  void InvokePendingQueries();

  // Returns the global First-Party Sets. This clones the underlying
  // data.
  //
  // Must be called after the list has been initialized.
  net::GlobalFirstPartySets GetGlobalSetsSync() const;

  // Performs the actual state clearing for the given context. Must not be
  // called until initialization is complete.
  void ClearSiteDataOnChangedSetsForContextInternal(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config,
      base::OnceCallback<void(net::FirstPartySetsContextConfig,
                              net::FirstPartySetsCacheFilter)> callback);

  // Like ComputeFirstPartySetMetadata, but passes the result into the provided
  // callback. Must not be called before `global_sets_` has been set.
  void ComputeFirstPartySetMetadataInternal(
      const net::SchemefulSite& site,
      const std::optional<net::SchemefulSite>& top_frame_site,
      const net::FirstPartySetsContextConfig& config,
      const base::ElapsedTimer& timer,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const;

  // Parses the policy and computes the config that represents the changes
  // needed to apply `policy` to `global_sets_`.
  net::FirstPartySetsContextConfig GetContextConfigForPolicyInternal(
      const base::Value::Dict& policy,
      const std::optional<base::ElapsedTimer>& timer) const;

  void OnGetSitesToClear(
      base::RepeatingCallback<BrowserContext*()> browser_context_getter,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config,
      base::OnceCallback<void(net::FirstPartySetsContextConfig,
                              net::FirstPartySetsCacheFilter)> callback,
      std::optional<std::pair<std::vector<net::SchemefulSite>,
                              net::FirstPartySetsCacheFilter>> sites_to_clear)
      const;

  // `failed_data_types` is a bitmask used to indicate data types from
  // BrowsingDataRemover::DataType enum that were failed to remove. 0 indicates
  // success.
  void DidClearSiteDataOnChangedSetsForContext(
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config,
      net::FirstPartySetsCacheFilter cache_filter,
      base::OnceCallback<void(net::FirstPartySetsContextConfig,
                              net::FirstPartySetsCacheFilter)> callback,
      uint64_t failed_data_types) const;

  // Whether Init has been called already or not.
  bool initialized_ = false;

  // The global First-Party Sets, after parsing and validation.
  //
  // This is nullopt until all of the required inputs have been received.
  std::optional<net::GlobalFirstPartySets> global_sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether the First-Party Sets feature should behave as "enabled" or not,
  // according to the embedder.
  const bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A queue of tasks waiting to run once this instance has the full
  // GlobalFirstPartySets instance. If `enabled_` is true, then this queue is
  // non-null until `global_sets_` is non-nullopt. Otherwise, it is always
  // nullptr.
  std::unique_ptr<base::circular_deque<base::OnceClosure>>
      on_sets_ready_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A helper object to handle loading and combining First-Party Sets from
  // different sources (i.e. the command-line flag and the list provided by the
  // embedder). This is nullptr if `enabled_` is false; and it is nullptr after
  // `global_sets_` has been set.
  std::unique_ptr<FirstPartySetsLoader> sets_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the first async task was enqueued, if any. Used for
  // metrics.
  std::optional<base::ElapsedTimer> first_async_task_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Access the underlying DB on a database sequence to make sure none of DB
  // operations that support blocking are called directly on the main thread.
  base::SequenceBound<FirstPartySetsHandlerDatabaseHelper> db_helper_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_INSTANCE_H_
