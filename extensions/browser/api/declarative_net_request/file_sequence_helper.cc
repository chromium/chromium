// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/rules_count_pair.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/error_utils.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;

// A class to help in re-indexing multiple rulesets.
class ReindexHelper : public base::RefCountedThreadSafe<ReindexHelper> {
 public:
  using ReindexCallback = base::OnceCallback<void(LoadRequestData)>;
  ReindexHelper(LoadRequestData data, ReindexCallback callback)
      : data_(std::move(data)), callback_(std::move(callback)) {}

  // Starts re-indexing rulesets. Must be called on the extension file task
  // runner.
  void Start() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    std::vector<RulesetInfo*> rulesets_to_reindex;
    for (auto& ruleset : data_.rulesets) {
      if (ruleset.did_load_successfully())
        continue;

      rulesets_to_reindex.push_back(&ruleset);
    }

    // |done_closure| will be invoked once |barrier_closure| is run
    // |rulesets_to_reindex.size()| times.
    base::OnceClosure done_closure =
        base::BindOnce(&ReindexHelper::OnAllRulesetsReindexed, this);
    base::RepeatingClosure barrier_closure = base::BarrierClosure(
        rulesets_to_reindex.size(), std::move(done_closure));

    // Post tasks to reindex individual rulesets.
    for (RulesetInfo* ruleset : rulesets_to_reindex) {
      auto callback = base::BindOnce(&ReindexHelper::OnReindexCompleted, this,
                                     ruleset, barrier_closure);
      ruleset->source().IndexAndPersistJSONRuleset(&decoder_,
                                                   std::move(callback));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ReindexHelper>;
  ~ReindexHelper() = default;

  // Callback invoked when reindexing of all rulesets is completed.
  void OnAllRulesetsReindexed() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    // Our job is done.
    std::move(callback_).Run(std::move(data_));
  }

  // Callback invoked when a single ruleset is re-indexed.
  void OnReindexCompleted(RulesetInfo* ruleset,
                          base::OnceClosure done_closure,
                          IndexAndPersistJSONRulesetResult result) {
    using IndexStatus = IndexAndPersistJSONRulesetResult::Status;
    DCHECK(ruleset);

    // The checksum of the reindexed ruleset should have been the same as the
    // expected checksum obtained from prefs, in all cases except when the
    // ruleset version changes. If this is not the case, then there is some
    // other issue (like the JSON rules file has been modified from the one used
    // during installation or preferences are corrupted). But taking care of
    // these is beyond our scope here, so simply signal a failure.
    bool reindexing_success =
        result.status == IndexStatus::kSuccess &&
        ruleset->expected_checksum() == result.ruleset_checksum;

    // In case of updates to the ruleset version, the change of ruleset checksum
    // is expected.
    if (result.status == IndexStatus::kSuccess &&
        ruleset->load_ruleset_result() ==
            LoadRulesetResult::kErrorVersionMismatch) {
      ruleset->set_new_checksum(result.ruleset_checksum);

      // Also change the |expected_checksum| so that any subsequent load
      // succeeds.
      ruleset->set_expected_checksum(result.ruleset_checksum);
      reindexing_success = true;
    }

    ruleset->set_reindexing_successful(reindexing_success);

    UMA_HISTOGRAM_BOOLEAN(
        "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful",
        reindexing_success);

    std::move(done_closure).Run();
  }

  LoadRequestData data_;
  ReindexCallback callback_;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this ReindexHelper.
  data_decoder::DataDecoder decoder_;

  DISALLOW_COPY_AND_ASSIGN(ReindexHelper);
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

  NOTREACHED();
  return UpdateDynamicRulesStatus::kSuccess;
}

// Helper to create the new list of dynamic rules. Returns false on failure and
// populates |error| and |status|.
bool GetNewDynamicRules(const FileBackedRulesetSource& source,
                        std::vector<int> rule_ids_to_remove,
                        std::vector<dnr_api::Rule> rules_to_add,
                        const RulesCountPair& rule_limit,
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
  base::EraseIf(*new_rules, [&ids_to_remove](const dnr_api::Rule& rule) {
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

  size_t regex_rule_count = std::count_if(
      new_rules->begin(), new_rules->end(),
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
                                const RulesCountPair& rule_limit,
                                int* ruleset_checksum,
                                std::string* error,
                                UpdateDynamicRulesStatus* status) {
  DCHECK(ruleset_checksum);
  DCHECK(error);
  DCHECK(status);

  std::set<int> rule_ids_to_add;
  for (const dnr_api::Rule& rule : rules_to_add)
    rule_ids_to_add.insert(rule.id);

  std::vector<dnr_api::Rule> new_rules;
  if (!GetNewDynamicRules(source, std::move(rule_ids_to_remove),
                          std::move(rules_to_add), rule_limit, &new_rules,
                          error, status)) {
    return false;  // |error| and |status| already populated.
  }

  // Initially write the new JSON and indexed rulesets to temporary files to
  // ensure we don't leave the actual files in an inconsistent state.
  std::unique_ptr<FileBackedRulesetSource> temporary_source =
      FileBackedRulesetSource::CreateTemporarySource(
          source.id(), source.rule_count_limit(), source.extension_id());
  if (!temporary_source) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorCreateTemporarySource;
    return false;
  }

  // Persist JSON.
  if (!temporary_source->WriteRulesToJSON(new_rules)) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorWriteTemporaryJSONRuleset;
    return false;
  }

  // Index and persist the indexed ruleset.
  ParseInfo info = temporary_source->IndexAndPersistRules(std::move(new_rules));
  if (info.has_error()) {
    *error = info.error();
    *status = info.error_reason() == ParseResult::ERROR_PERSISTING_RULESET
                  ? UpdateDynamicRulesStatus::kErrorWriteTemporaryIndexedRuleset
                  : UpdateDynamicRulesStatus::kErrorInvalidRules;
    return false;
  }

  *ruleset_checksum = info.ruleset_checksum();

  // Treat rules which exceed the regex memory limit as errors if these are new
  // rules. Just surface an error for the first such rule.
  for (int rule_id : info.regex_limit_exceeded_rules()) {
    if (!base::Contains(rule_ids_to_add, rule_id)) {
      // Any rule added earlier which is ignored now (say due to exceeding the
      // regex memory limit), will be silently ignored.
      // TODO(crbug.com/1050780): Notify the extension about the same.
      continue;
    }

    *error = ErrorUtils::FormatErrorMessage(
        kErrorRegexTooLarge, base::NumberToString(rule_id), kRegexFilterKey);
    *status = UpdateDynamicRulesStatus::kErrorRegexTooLarge;
    return false;
  }

  // Dynamic JSON and indexed rulesets for an extension are stored in the same
  // directory.
  DCHECK_EQ(source.indexed_path().DirName(), source.json_path().DirName());

  // Place the indexed ruleset at the correct location. base::ReplaceFile should
  // involve a rename and ideally be atomic at the system level. Before doing so
  // ensure that the destination directory exists, since this is not handled by
  // base::ReplaceFile.
  if (!base::CreateDirectory(source.indexed_path().DirName())) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorCreateDynamicRulesDirectory;
    return false;
  }

  // TODO(karandeepb): ReplaceFile can fail if the source and destination files
  // are on different volumes. Investigate if temporary files can be created on
  // a different volume than the profile path.
  if (!base::ReplaceFile(temporary_source->indexed_path(),
                         source.indexed_path(), nullptr /* error */)) {
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorReplaceIndexedFile;
    return false;
  }

  // Place the json ruleset at the correct location.
  if (!base::ReplaceFile(temporary_source->json_path(), source.json_path(),
                         nullptr /* error */)) {
    // We have entered into an inconsistent state where the indexed ruleset was
    // updated but not the JSON ruleset. This should be extremely rare. However
    // if we get here, the next time the extension is loaded, we'll identify
    // that the indexed ruleset checksum is inconsistent and reindex the JSON
    // ruleset.
    *error = kInternalErrorUpdatingDynamicRules;
    *status = UpdateDynamicRulesStatus::kErrorReplaceJSONFile;
    return false;
  }

  return true;  // |ruleset_checksum| already populated.
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

const absl::optional<LoadRulesetResult>& RulesetInfo::load_ruleset_result()
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

LoadRequestData::LoadRequestData(ExtensionId extension_id)
    : extension_id(std::move(extension_id)) {}
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

  // Loading one or more rulesets failed. Re-index them.

  // Using a WeakPtr is safe since |reindex_callback| will be called on this
  // sequence itself.
  auto reindex_callback =
      base::BindOnce(&FileSequenceHelper::OnRulesetsReindexed,
                     weak_factory_.GetWeakPtr(), std::move(ui_callback));

  auto reindex_helper = base::MakeRefCounted<ReindexHelper>(
      std::move(load_data), std::move(reindex_callback));
  reindex_helper->Start();
}

void FileSequenceHelper::UpdateDynamicRules(
    LoadRequestData load_data,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    const RulesCountPair& rule_limit,
    UpdateDynamicRulesUICallback ui_callback) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(1u, load_data.rulesets.size());

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK(!dynamic_ruleset.expected_checksum());

  auto log_status_and_dispatch_callback = [&ui_callback, &load_data](
                                              absl::optional<std::string> error,
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
  log_status_and_dispatch_callback(absl::nullopt, status);
}

void FileSequenceHelper::OnRulesetsReindexed(LoadRulesetsUICallback ui_callback,
                                             LoadRequestData load_data) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Load rulesets for which reindexing succeeded.
  for (auto& ruleset : load_data.rulesets) {
    if (ruleset.reindexing_successful().value_or(false)) {
      // Only rulesets which can't be loaded are re-indexed.
      DCHECK(!ruleset.did_load_successfully());
      ruleset.CreateVerifiedMatcher();
    }
  }

  // The UI thread will handle success or failure.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(ui_callback), std::move(load_data)));
}

}  // namespace declarative_net_request
}  // namespace extensions
