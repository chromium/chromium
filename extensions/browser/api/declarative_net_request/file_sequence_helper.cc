// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions::declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;

// A class to help in indexing multiple rulesets.
// TODO(crbug.com/40794487): Look into unifying this with the InstallIndexHelper
//                          class, moving any differing logic to the clients.
class IndexHelper : public base::RefCountedThreadSafe<IndexHelper> {
 public:
  using IndexCallback = base::OnceCallback<void(LoadRequestData)>;
  IndexHelper(LoadRequestData data, IndexCallback callback)
      : data_(std::move(data)), callback_(std::move(callback)) {}

  IndexHelper(const IndexHelper&) = delete;
  IndexHelper& operator=(const IndexHelper&) = delete;

  // Starts indexing rulesets. Must be called on the extension file task runner.
  void Start(uint8_t parse_flags) {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    std::vector<RulesetInfo*> rulesets_to_index;
    for (auto& ruleset : data_.rulesets) {
      if (ruleset.did_load_successfully()) {
        continue;
      }

      rulesets_to_index.push_back(&ruleset);
    }

    // `done_closure` will be invoked once `barrier_closure` is run
    // `rulesets_to_index.size()` times.
    base::OnceClosure done_closure =
        base::BindOnce(&IndexHelper::OnAllRulesetsIndexed, this);
    base::RepeatingClosure barrier_closure =
        base::BarrierClosure(rulesets_to_index.size(), std::move(done_closure));

    // Post tasks to index individual rulesets.
    for (RulesetInfo* ruleset : rulesets_to_index) {
      auto callback = base::BindOnce(&IndexHelper::OnIndexCompleted, this,
                                     ruleset, barrier_closure);
      ruleset->source().IndexAndPersistJSONRuleset(&decoder_, parse_flags,
                                                   std::move(callback));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<IndexHelper>;
  ~IndexHelper() = default;

  // Callback invoked when indexing of all rulesets is completed.
  void OnAllRulesetsIndexed() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    // Our job is done.
    std::move(callback_).Run(std::move(data_));
  }

  // Callback invoked when a single ruleset is indexed.
  void OnIndexCompleted(RulesetInfo* ruleset,
                        base::OnceClosure done_closure,
                        IndexAndPersistJSONRulesetResult result) {
    using IndexStatus = IndexAndPersistJSONRulesetResult::Status;
    DCHECK(ruleset);

    bool indexing_success = result.status == IndexStatus::kSuccess;
    bool is_reindexing = ruleset->expected_checksum().has_value();
    if (indexing_success) {
      // If this is the first time that the ruleset is being indexed, or if the
      // ruleset's version has updated, then take note of the new checksum.
      bool update_checksum =
          !is_reindexing || ruleset->load_ruleset_result() ==
                                LoadRulesetResult::kErrorVersionMismatch;
      if (update_checksum) {
        ruleset->set_new_checksum(result.ruleset_checksum);

        // Also change the `expected_checksum` so that any subsequent load
        // succeeds.
        ruleset->set_expected_checksum(result.ruleset_checksum);
      } else {
        // Otherwise, the checksum of the re-indexed ruleset should match the
        // expected checksum. If this is not the case, then there is some other
        // issue (like the JSON rules file has been modified from the one used
        // during installation or preferences are corrupted). But taking care of
        // these is beyond our scope here, so simply signal a failure.
        indexing_success =
            ruleset->expected_checksum() == result.ruleset_checksum;
      }
    }

    ruleset->set_indexing_successful(indexing_success);

    if (is_reindexing) {
      UMA_HISTOGRAM_BOOLEAN(
          "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
          indexing_success);
    }

    std::move(done_closure).Run();
  }

  LoadRequestData data_;
  IndexCallback callback_;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this IndexHelper.
  data_decoder::DataDecoder decoder_;
};

UpdateDynamicRulesStatus GetUpdateDynamicRuleStatus(LoadRulesetResult result) {
  switch (result) {
    case LoadRulesetResult::kSuccess:
      break;
    case LoadRulesetResult::kErrorInvalidPath:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_InvalidPath;
    case LoadRulesetResult::kErrorCannotReadFile:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_FileReadError;
    case LoadRulesetResult::kErrorChecksumMismatch:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_ChecksumMismatch;
    case LoadRulesetResult::kErrorVersionMismatch:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_VersionMismatch;
    case LoadRulesetResult::kErrorChecksumNotFound:
      // Updating dynamic rules shouldn't require looking up checksum from
      // prefs.
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return UpdateDynamicRulesStatus::kSuccess;
}

// Helper to create the new list of dynamic rules. Returns false on failure and
// populates |error| and |status|.
bool GetNewDynamicRules(const FileBackedRulesetSource& source,
                        std::vector<int> rule_ids_to_remove,
                        std::vector<dnr_api::Rule> rules_to_add,
                        const RuleCounts& rule_limit,
                        std::vector<dnr_api::Rule>* new_rules,
                        std::string* error,
                        UpdateDynamicRulesStatus* status) {
  DCHECK(new_rules);
  DCHECK(error);
  DCHECK(status);

  // Read the current set of rules. Note: this is trusted JSON and hence it is
  // ok to parse in the browser itself.
  ReadJSONRulesResult result = source.ReadJSONRulesUnsafe();
  LogReadDynamicRulesStatus(result.status);
  DCHECK(result.status == ReadJSONRulesResult::Status::kSuccess ||
         result.rules.empty());

  // Possible cases:
  // - kSuccess
  // - kFileDoesNotExist: This can happen when persisting dynamic rules for the
  //   first time.
  // - kFileReadError: Throw an internal error.
  // - kJSONParseError, kJSONIsNotList: These denote JSON ruleset corruption.
  //   Assume the current set of rules is empty.
  if (result.status == ReadJSONRulesResult::Status::kFileReadError) {
    *status = UpdateDynamicRulesStatus::kErrorReadJSONRules;
    *error = kInternalErrorUpdatingDynamicRules;
    return false;
  }

  *new_rules = std::move(result.rules);

  // Remove old rules
  std::set<int> ids_to_remove(rule_ids_to_remove.begin(), rule_ids_to_remove.end());
  std::erase_if(*new_rules, [&ids_to_remove](const dnr_api::Rule& rule) {
    return base::Contains(ids_to_remove, rule.id);
  });

  // Add new rules
  new_rules->insert(new_rules->end(),
                    std::make_move_iterator(rules_to_add.begin()),
                    std::make_move_iterator(rules_to_add.end()));

  if (new_rules->size() > rule_limit.rule_count) {
    *status = UpdateDynamicRulesStatus::kErrorRuleCountExceeded;
    *error = kDynamicRuleCountExceeded;
    return false;
  }

  if (base::FeatureList::IsEnabled(
          extensions_features::kDeclarativeNetRequestSafeRuleLimits)) {
    size_t unsafe_rule_count = base::ranges::count_if(
        *new_rules,
        [](const dnr_api::Rule& rule) { return !IsRuleSafe(rule); });
    if (unsafe_rule_count > rule_limit.unsafe_rule_count) {
      *status = UpdateDynamicRulesStatus::kErrorUnsafeRuleCountExceeded;
      *error = kDynamicUnsafeRuleCountExceeded;
      return false;
    }
  }

  size_t regex_rule_count = base::ranges::count_if(
      *new_rules,
      [](const dnr_api::Rule& rule) { return !!rule.condition.regex_filter; });
  if (regex_rule_count > rule_limit.regex_rule_count) {
    *status = UpdateDynamicRulesStatus::kErrorRegexRuleCountExceeded;
    *error = kDynamicRegexRuleCountExceeded;
    return false;
  }

  return true;
}

// Returns true on success and populates |ruleset_checksum|. Returns false on
// failure and populates |error| and |status|.
bool UpdateAndIndexDynamicRules(const FileBackedRulesetSource& source,
                                std::vector<int> rule_ids_to_remove,
                                std::vector<dnr_api::Rule> rules_to_add,
                                const RuleCounts& rule_limit,
                                int* ruleset_checksum,
                                std::string* error,
                                UpdateDynamicRulesStatus* status) {
  DCHECK(ruleset_checksum);
  DCHECK(error);
  DCHECK(status);

  // Dynamic JSON and indexed rulesets for an extension are stored in the same
  // directory.
  DCHECK_EQ(source.indexed_path().DirName(), source.json_path().DirName());

  std::set<int> rule_ids_to_add;
  for (const dnr_api::Rule& rule : rules_to_add) {
    rule_ids_to_add.insert(rule.id);
  }

  std::vector<dnr_api::Rule> new_rules;
  if (!GetNewDynamicRules(source, std::move(rule_ids_to_remove),
                          std::move(rules_to_add), rule_limit, &new_rules,
                          error, status)) {
    return false;  // |error| and |status| already populated.
  }

  // Serialize rules to JSON.
  std::string json;
  if (!source.SerializeRulesToJSON(new_rules, &json)) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorSerializeToJson;
    return false;
  }

  // Index rules.
  auto parse_flags = RulesetSource::kRaiseErrorOnInvalidRules |
                     RulesetSource::kRaiseWarningOnLargeRegexRules;
  ParseInfo info = source.IndexRules(std::move(new_rules), parse_flags);

  if (info.has_error()) {
    *error = info.error();
    *status = UpdateDynamicRulesStatus::kErrorInvalidRules;
    return false;
  }

  // Treat rules which exceed the regex memory limit as errors if these are new
  // rules. Just surface an error for the first such rule.
  for (auto warning : info.rule_ignored_warnings()) {
    if (!base::Contains(rule_ids_to_add, warning.rule_id)) {
      // Any rule added earlier which is ignored now (say due to exceeding the
      // regex memory limit), will be silently ignored.
      // TODO(crbug.com/40118204): Notify the extension about the same.
      continue;
    }

    *error = warning.message;
    *status = UpdateDynamicRulesStatus::kErrorRegexTooLarge;
    return false;
  }

  // Ensure that the destination directory exists.
  if (!base::CreateDirectory(source.indexed_path().DirName())) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorCreateDynamicRulesDirectory;
    return false;
  }

  // Persist indexed ruleset. Use `ImportantFileWriter` to make this atomic and
  // decrease the likelihood of file corruption.
  if (!base::ImportantFileWriter::WriteFileAtomically(
          source.indexed_path(), GetIndexedRulesetData(info.GetBuffer()),
          "DNRDynamicRulesFlatbuffer")) {
    // If this fails, we might have corrupted the existing indexed ruleset file.
    // However the JSON source of truth hasn't been modified. The next time the
    // extension is loaded, the indexed ruleset will fail checksum verification
    // leading to reindexing of the JSON ruleset.
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorWriteFlatbuffer;
    return false;
  }

  // Persist JSON. Since the JSON ruleset is the source of truth, use
  // `ImportantFileWriter` to make this atomic and decrease the likelihood of
  // file corruption.
  if (!base::ImportantFileWriter::WriteFileAtomically(
          source.json_path(), json, "DNRDynamicRulesetJson")) {
    // We have entered into an inconsistent state where the indexed ruleset was
    // updated but not the JSON ruleset. This should be extremely rare. However
    // if we get here, the next time the extension is loaded, we'll identify
    // that the indexed ruleset checksum is inconsistent and re-index the JSON
    // ruleset.
    // If the JSON ruleset is corrupted here though, loading the dynamic ruleset
    // subsequently will fail. A call by extension to `updateDynamicRules`
    // should help it start from a clean slate in this case (See
    // `GetNewDynamicRules` above).
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorWriteJson;
    return false;
  }

  *ruleset_checksum = info.ruleset_checksum();
  return true;
}

}  // namespace

RulesetInfo::RulesetInfo(FileBackedRulesetSource source)
    : source_(std::move(source)) {}
RulesetInfo::~RulesetInfo() = default;
RulesetInfo::RulesetInfo(RulesetInfo&&) = default;
RulesetInfo& RulesetInfo::operator=(RulesetInfo&&) = default;

std::unique_ptr<RulesetMatcher> RulesetInfo::TakeMatcher() {
  DCHECK(did_load_successfully());
  return std::move(matcher_);
}

const std::optional<LoadRulesetResult>& RulesetInfo::load_ruleset_result()
    const {
  // |matcher_| is valid only on success.
  DCHECK_EQ(load_ruleset_result_ == LoadRulesetResult::kSuccess, !!matcher_);
  return load_ruleset_result_;
}

void RulesetInfo::CreateVerifiedMatcher() {
  DCHECK(expected_checksum_);
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Ensure we aren't calling this redundantly. If did_load_successfully()
  // returns true, we should already have a valid RulesetMatcher.
  DCHECK(!did_load_successfully());

  load_ruleset_result_ =
      source_.CreateVerifiedMatcher(*expected_checksum_, &matcher_);
}

LoadRequestData::LoadRequestData(ExtensionId extension_id,
                                 base::Version extension_version)
    : extension_id(std::move(extension_id)),
      extension_version(std::move(extension_version)) {}
LoadRequestData::~LoadRequestData() = default;
LoadRequestData::LoadRequestData(LoadRequestData&&) = default;
LoadRequestData& LoadRequestData::operator=(LoadRequestData&&) = default;

FileSequenceHelper::FileSequenceHelper() = default;

FileSequenceHelper::~FileSequenceHelper() {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
}

void FileSequenceHelper::LoadRulesets(
    LoadRequestData load_data,
    LoadRulesetsUICallback ui_callback) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  bool success = true;
  for (auto& ruleset : load_data.rulesets) {
    if (!ruleset.expected_checksum()) {
      // This ruleset hasn't been indexed yet.
      success = false;
      continue;
    }

    ruleset.CreateVerifiedMatcher();
    success &= ruleset.did_load_successfully();
  }

  if (success) {
    // Set priority explicitly to avoid unwanted task priority inheritance.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
        ->PostTask(FROM_HERE, base::BindOnce(std::move(ui_callback),
                                             std::move(load_data)));
    return;
  }

  // Not all rulesets were loaded. This can be because some rulesets haven't
  // been indexed previously or because indexing failed for a ruleset. Try
  // indexing these rulesets now.

  // Ignore invalid static rules during deferred indexing or while re-indexing.
  auto parse_flags = RulesetSource::kNone;

  // Using a WeakPtr is safe since `index_callback` will be called on this
  // sequence itself.
  auto index_callback =
      base::BindOnce(&FileSequenceHelper::OnRulesetsIndexed,
                     weak_factory_.GetWeakPtr(), std::move(ui_callback));

  auto index_helper = base::MakeRefCounted<IndexHelper>(
      std::move(load_data), std::move(index_callback));
  index_helper->Start(parse_flags);
}

void FileSequenceHelper::UpdateDynamicRules(
    LoadRequestData load_data,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    const RuleCounts& rule_limit,
    UpdateDynamicRulesUICallback ui_callback) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(1u, load_data.rulesets.size());

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK(!dynamic_ruleset.expected_checksum());

  auto log_status_and_dispatch_callback = [&ui_callback, &load_data](
                                              std::optional<std::string> error,
                                              UpdateDynamicRulesStatus status) {
    base::UmaHistogramEnumeration(kUpdateDynamicRulesStatusHistogram, status);

    // Set priority explicitly to avoid unwanted task priority inheritance.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
        ->PostTask(FROM_HERE,
                   base::BindOnce(std::move(ui_callback), std::move(load_data),
                                  std::move(error)));
  };

  int new_ruleset_checksum = -1;
  std::string error;
  UpdateDynamicRulesStatus status = UpdateDynamicRulesStatus::kSuccess;
  if (!UpdateAndIndexDynamicRules(dynamic_ruleset.source(),
                                  std::move(rule_ids_to_remove),
                                  std::move(rules_to_add), rule_limit,
                                  &new_ruleset_checksum, &error, &status)) {
    DCHECK(!error.empty());
    log_status_and_dispatch_callback(std::move(error), status);
    return;
  }

  DCHECK_EQ(UpdateDynamicRulesStatus::kSuccess, status);
  dynamic_ruleset.set_expected_checksum(new_ruleset_checksum);
  dynamic_ruleset.set_new_checksum(new_ruleset_checksum);
  dynamic_ruleset.CreateVerifiedMatcher();
  DCHECK(dynamic_ruleset.load_ruleset_result());

  if (!dynamic_ruleset.did_load_successfully()) {
    status = GetUpdateDynamicRuleStatus(*dynamic_ruleset.load_ruleset_result());
    log_status_and_dispatch_callback(kInternalErrorUpdatingDynamicRules,
                                     status);
    return;
  }

  // Success.
  log_status_and_dispatch_callback(std::nullopt, status);
}

void FileSequenceHelper::OnRulesetsIndexed(LoadRulesetsUICallback ui_callback,
                                           LoadRequestData load_data) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Load rulesets for which indexing succeeded.
  for (auto& ruleset : load_data.rulesets) {
    if (ruleset.indexing_successful().value_or(false)) {
      // Only rulesets which weren't indexed previously or for which loading
      // failed are being indexed.
      DCHECK(!ruleset.did_load_successfully());
      ruleset.CreateVerifiedMatcher();
    }
  }

  // The UI thread will handle success or failure.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(ui_callback), std::move(load_data)));
}

}  // namespace extensions::declarative_net_request
