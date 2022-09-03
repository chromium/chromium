// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/common/content_export.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Class FirstPartySetsHandlerImpl is a singleton, it allows an embedder to
// provide First-Party Sets inputs from custom sources, then parses/merges the
// inputs to form the current First-Party Sets data, compares them with the
// persisted First-Party Sets data used during the last browser session to get
// a list of sites that changed the First-Party Set they are part of, invokes
// the provided callback with the current First-Party Sets data, and writes
// the current First-Party Sets data to disk.
class CONTENT_EXPORT FirstPartySetsHandlerImpl : public FirstPartySetsHandler {
 public:
  using FlattenedSets =
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>;
  using SetsReadyOnceCallback =
      base::OnceCallback<void(network::mojom::PublicFirstPartySetsPtr)>;
  using PolicyCustomization = FirstPartySetsHandler::PolicyCustomization;

  static FirstPartySetsHandlerImpl* GetInstance();

  ~FirstPartySetsHandlerImpl() override;

  FirstPartySetsHandlerImpl(const FirstPartySetsHandlerImpl&) = delete;
  FirstPartySetsHandlerImpl& operator=(const FirstPartySetsHandlerImpl&) =
      delete;

  // This method reads the persisted First-Party Sets from the file under
  // `user_data_dir` and sets the First-Party Set that was provided via the
  // flag/switch.
  //
  // If First-Party Sets is disabled, then this method still needs to read the
  // persisted sets, since we may still need to clear data from a previous
  // invocation of Chromium which had First-Party Sets enabled.
  //
  // Must be called exactly once.
  void Init(const base::FilePath& user_data_dir, const std::string& flag_value);

  // Returns the fully-parsed and validated public First-Party Sets data.
  // Returns the data synchronously via an optional if it's already available,
  // or via an asynchronously-invoked callback if the data is not ready yet.
  //
  // This function makes a clone of the public First-Party Sets.
  //
  // If `callback` is null, it will not be invoked, even if the First-Party Sets
  // data is not ready yet.
  //
  // Must not be called if First-Party Sets is disabled.
  [[nodiscard]] absl::optional<network::mojom::PublicFirstPartySetsPtr> GetSets(
      SetsReadyOnceCallback callback);

  // FirstPartySetsHandler
  bool IsEnabled() const override;
  void SetPublicFirstPartySets(base::File sets_file) override;
  void ResetForTesting() override;
  void GetCustomizationForPolicy(
      const base::Value::Dict& policy,
      base::OnceCallback<void(PolicyCustomization)> callback) override;

  // Sets whether FPS is enabled (for testing).
  void SetEnabledForTesting(bool enabled) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    enabled_ = enabled;
  }

  void SetEmbedderWillProvidePublicSetsForTesting(bool will_provide) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    embedder_will_provide_public_sets_ = enabled_ && will_provide;
  }

  void GetPersistedPublicSetsForTesting(
      base::OnceCallback<void(FirstPartySetsHandlerImpl::FlattenedSets)>
          callback);

  // Computes information needed by the FirstPartySetsAccessDelegate in order
  // to update the browser's list of First-Party Sets to respect a profile's
  // setting for the per-profile FirstPartySetsOverrides policy.
  static PolicyCustomization ComputeEnterpriseCustomizations(
      const network::mojom::PublicFirstPartySetsPtr& public_sets,
      const FirstPartySetParser::ParsedPolicySetLists& policy);

 private:
  friend class base::NoDestructor<FirstPartySetsHandlerImpl>;

  FirstPartySetsHandlerImpl(bool enabled,
                            bool embedder_will_provide_public_sets);

  // Sets the public First-Party Sets data. Must be called exactly once.
  void SetCompleteSets(network::mojom::PublicFirstPartySetsPtr public_sets);

  // Sets `db_helper_`, which will initialize the underlying First-Party Sets
  // database under `user_data_dir`. Must be called exactly once.
  void SetDatabase(const base::FilePath& user_data_dir);

  // Invokes any pending queries.
  void InvokePendingQueries();

  // Returns the list of public First-Party Sets.
  //
  // Must be called after the list has been initialized.
  network::mojom::PublicFirstPartySetsPtr GetSetsSync() const;

  // TODO(shuuran@chromium.org): Implement the code to clear site state.
  void ClearSiteDataOnChangedSets() const;

  // Parses the policy and computes the PolicyCustomization that represents the
  // changes needed to apply `policy` to `sets_`.
  PolicyCustomization GetCustomizationForPolicyInternal(
      const base::Value::Dict& policy) const;

  // Whether Init has been called already or not.
  bool initialized_ = false;

  // The public First-Party Sets, after parsing and validation.
  //
  // This is nullptr until all of the required inputs have been received.
  network::mojom::PublicFirstPartySetsPtr public_sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool embedder_will_provide_public_sets_ GUARDED_BY_CONTEXT(sequence_checker_);

  // We use a OnceCallback to ensure we only pass along the sets once
  // during Chrome's lifetime (modulo reconfiguring the network service).
  base::circular_deque<base::OnceClosure> on_sets_ready_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<FirstPartySetsLoader> sets_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when the instance is constructed. Used for metrics.
  base::ElapsedTimer construction_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Access the underlying DB on a database sequence to make sure none of DB
  // operations that support blocking are called directly on the main thread.
  base::SequenceBound<FirstPartySetsHandlerDatabaseHelper> db_helper_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_IMPL_H_
