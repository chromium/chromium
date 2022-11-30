// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_SEQUENCE_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_SEQUENCE_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace api {
namespace declarative_net_request {
struct Rule;
}  // namespace declarative_net_request
}  // namespace api

namespace declarative_net_request {
enum class DynamicRuleUpdateAction;
struct RulesCountPair;

// Holds the data relating to the loading of a single ruleset.
class RulesetInfo {
 public:
  explicit RulesetInfo(FileBackedRulesetSource source);
  RulesetInfo(const RulesetInfo&) = delete;
  RulesetInfo(RulesetInfo&&);

  RulesetInfo& operator=(const RulesetInfo&) = delete;
  RulesetInfo& operator=(RulesetInfo&&);

  ~RulesetInfo();

  const FileBackedRulesetSource& source() const { return source_; }

  // Returns the ownership of the ruleset matcher to the caller. Must only be
  // called for a successful load.
  std::unique_ptr<RulesetMatcher> TakeMatcher();

  // Clients should set a new checksum if the checksum stored in prefs should
  // be updated.
  void set_new_checksum(int new_checksum) { new_checksum_ = new_checksum; }
  absl::optional<int> new_checksum() const { return new_checksum_; }

  // The expected checksum for the indexed ruleset.
  void set_expected_checksum(int checksum) { expected_checksum_ = checksum; }
  absl::optional<int> expected_checksum() const { return expected_checksum_; }

  // Whether indexing of the ruleset was successful.
  void set_indexing_successful(bool val) { indexing_successful_ = val; }
  absl::optional<bool> indexing_successful() const {
    return indexing_successful_;
  }

  // Returns the result of loading the ruleset. The return value is valid (not
  // equal to absl::nullopt) iff CreateVerifiedMatcher() has been called.
  const absl::optional<LoadRulesetResult>& load_ruleset_result() const;

  // Whether the ruleset loaded successfully.
  bool did_load_successfully() const {
    return load_ruleset_result() == LoadRulesetResult::kSuccess;
  }

  // Must be invoked on the extension file task runner. Must only be called
  // after the expected checksum is set.
  void CreateVerifiedMatcher();

 private:
  FileBackedRulesetSource source_;

  // The expected checksum of the indexed ruleset.
  absl::optional<int> expected_checksum_;

  // Stores the result of creating a verified matcher from the |source_|.
  std::unique_ptr<RulesetMatcher> matcher_;
  absl::optional<LoadRulesetResult> load_ruleset_result_;

  // The new checksum to be persisted to prefs. A new checksum should only be
  // set in case of flatbuffer version mismatch.
  absl::optional<int> new_checksum_;

  // Whether the indexing of this ruleset was successful.
  absl::optional<bool> indexing_successful_;
};

// Helper to pass information related to the ruleset being loaded.
struct LoadRequestData {
  explicit LoadRequestData(ExtensionId extension_id);
  LoadRequestData(const LoadRequestData&) = delete;
  LoadRequestData(LoadRequestData&&);

  LoadRequestData& operator=(const LoadRequestData&) = delete;
  LoadRequestData& operator=(LoadRequestData&&);

  ~LoadRequestData();

  ExtensionId extension_id;
  std::vector<RulesetInfo> rulesets;
};

//  Helper class for file sequence operations for the declarative net request
//  API. Can be created on any sequence but must be used on the extension file
//  task runner.
class FileSequenceHelper {
 public:
  FileSequenceHelper();

  FileSequenceHelper(const FileSequenceHelper&) = delete;
  FileSequenceHelper& operator=(const FileSequenceHelper&) = delete;

  ~FileSequenceHelper();

  // Loads rulesets for `load_data`. Invokes `ui_callback` on the UI thread once
  // loading is done. Indexes the rulesets if they aren't yet indexed and also
  // tries to re-index the rulesets on failure. This is a no-op if
  // `load_data.rulesets` is empty.
  using LoadRulesetsUICallback = base::OnceCallback<void(LoadRequestData)>;
  void LoadRulesets(LoadRequestData load_data,
                    LoadRulesetsUICallback ui_callback) const;

  // Updates dynamic rules for |load_data|. Invokes |ui_callback| on the UI
  // thread once loading is done with the LoadRequestData and an optional error
  // string.
  using UpdateDynamicRulesUICallback =
      base::OnceCallback<void(LoadRequestData, absl::optional<std::string>)>;
  void UpdateDynamicRules(
      LoadRequestData load_data,
      std::vector<int> rule_ids_to_remove,
      std::vector<api::declarative_net_request::Rule> rules_to_add,
      const RulesCountPair& rule_limit,
      UpdateDynamicRulesUICallback ui_callback) const;

 private:
  // Callback invoked when the JSON rulesets are indexed.
  void OnRulesetsIndexed(LoadRulesetsUICallback ui_callback,
                         LoadRequestData load_data) const;

  // Must be the last member variable. See WeakPtrFactory documentation for
  // details. Mutable to allow GetWeakPtr() usage from const methods.
  mutable base::WeakPtrFactory<FileSequenceHelper> weak_factory_{this};
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_SEQUENCE_HELPER_H_
