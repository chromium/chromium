// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_sequence_helper.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/api/declarative_net_request.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;

// A class to help in re-indexing multiple rulesets.
class ReindexHelper {
 public:
  // Starts re-indexing rulesets. Must be called on the extension file task
  // runner.
  using ReindexCallback = base::OnceCallback<void(LoadRequestData)>;
  static void Start(LoadRequestData data, ReindexCallback callback) {
    auto* helper = new ReindexHelper(std::move(data), std::move(callback));
    helper->Start();
  }

 private:
  // We manage our own lifetime.
  ReindexHelper(LoadRequestData data, ReindexCallback callback)
      : data_(std::move(data)), callback_(std::move(callback)) {}
  ~ReindexHelper() = default;

  void Start() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

    // Post tasks to reindex individual rulesets.
    bool did_post_task = false;
    for (auto& ruleset : data_.rulesets) {
      if (ruleset.did_load_successfully())
        continue;

      // Using Unretained is safe since this class manages its own lifetime and
      // |this| won't be deleted until the |callback| returns.
      auto callback = base::BindOnce(&ReindexHelper::OnReindexCompleted,
                                     base::Unretained(this), &ruleset);
      callback_count_++;
      did_post_task = true;
      ruleset.source().IndexAndPersistJSONRuleset(&decoder_,
                                                  std::move(callback));
    }

    // It's possible that the callbacks return synchronously and we are deleted
    // at this point. Hence don't use any member variables here. Also, if we
    // don't post any task, we'll leak. Ensure that's not the case.
    DCHECK(did_post_task);
  }

  // Callback invoked when a single ruleset is re-indexed.
  void OnReindexCompleted(RulesetInfo* ruleset,
                          IndexAndPersistJSONRulesetResult result) {
    DCHECK(ruleset);

    // The checksum of the reindexed ruleset should have been the same as the
    // expected checksum obtained from prefs, in all cases except when the
    // ruleset version changes. If this is not the case, then there is some
    // other issue (like the JSON rules file has been modified from the one used
    // during installation or preferences are corrupted). But taking care of
    // these is beyond our scope here, so simply signal a failure.
    bool reindexing_success = result.success && ruleset->expected_checksum() ==
                                                    result.ruleset_checksum;

    // In case of updates to the ruleset version, the change of ruleset checksum
    // is expected.
    if (result.success &&
        ruleset->load_ruleset_result() ==
            RulesetMatcher::LoadRulesetResult::kLoadErrorVersionMismatch) {
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

    callback_count_--;
    DCHECK_GE(callback_count_, 0);

    if (callback_count_ == 0) {
      // Our job is done.
      std::move(callback_).Run(std::move(data_));
      delete this;
    }
  }

  LoadRequestData data_;
  ReindexCallback callback_;
  int callback_count_ = 0;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this ReindexHelper.
  data_decoder::DataDecoder decoder_;

  DISALLOW_COPY_AND_ASSIGN(ReindexHelper);
};

UpdateDynamicRulesStatus GetStatusForLoadRulesetError(
    RulesetMatcher::LoadRulesetResult result) {
  using Result = RulesetMatcher::LoadRulesetResult;
  switch (result) {
    case Result::kLoadSuccess:
      break;
    case Result::kLoadErrorInvalidPath:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_InvalidPath;
    case Result::kLoadErrorFileRead:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_FileReadError;
    case Result::kLoadErrorChecksumMismatch:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_ChecksumMismatch;
    case Result::kLoadErrorVersionMismatch:
      return UpdateDynamicRulesStatus::kErrorCreateMatcher_VersionMismatch;
    case Result::kLoadResultMax:
      break;
  }

  NOTREACHED();
  return UpdateDynamicRulesStatus::kSuccess;
}

// Helper to create the new list of dynamic rules. Returns false on failure and
// populates |error| and |status|.
bool GetNewDynamicRules(const RulesetSource& source,
                        std::vector<int> rule_ids_to_remove,
                        std::vector<dnr_api::Rule> rules_to_add,
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

  if (new_rules->size() > source.rule_count_limit()) {
    *status = UpdateDynamicRulesStatus::kErrorRuleCountExceeded;
    *error = kDynamicRuleCountExceeded;
    return false;
  }

  return true;
}

// Returns true on success and populates |ruleset_checksum|. Returns false on
// failure and populates |error| and |status|.
bool UpdateAndIndexDynamicRules(
    const RulesetSource& source,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    int* ruleset_checksum,
    std::string* error,
    UpdateDynamicRulesStatus* status) {
  DCHECK(ruleset_checksum);
  DCHECK(error);
  DCHECK(status);

  std::vector<dnr_api::Rule> new_rules;
  if (!GetNewDynamicRules(source, std::move(rule_ids_to_remove),
                          std::move(rules_to_add), &new_rules, error, status)) {
    return false;  // |error| and |status| already populated.
  }

  // Initially write the new JSON and indexed rulesets to temporary files to
  // ensure we don't leave the actual files in an inconsistent state.
  std::unique_ptr<RulesetSource> temporary_source =
      RulesetSource::CreateTemporarySource(
          source.id(), source.priority(), source.type(),
          source.rule_count_limit(), source.extension_id());
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
  ParseInfo info = temporary_source->IndexAndPersistRules(std::move(new_rules),
                                                          ruleset_checksum);
  if (info.result() != ParseResult::SUCCESS) {
    *error = info.GetErrorDescription();
    *status = info.result() == ParseResult::ERROR_PERSISTING_RULESET
                  ? UpdateDynamicRulesStatus::kErrorWriteTemporaryIndexedRuleset
                  : UpdateDynamicRulesStatus::kErrorInvalidRules;
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

RulesetInfo::RulesetInfo(RulesetSource source) : source_(std::move(source)) {}
RulesetInfo::~RulesetInfo() = default;
RulesetInfo::RulesetInfo(RulesetInfo&&) = default;
RulesetInfo& RulesetInfo::operator=(RulesetInfo&&) = default;

std::unique_ptr<RulesetMatcher> RulesetInfo::TakeMatcher() {
  DCHECK(did_load_successfully());
  return std::move(matcher_);
}

RulesetMatcher::LoadRulesetResult RulesetInfo::load_ruleset_result() const {
  DCHECK(load_ruleset_result_);
  // |matcher_| is valid only on success.
  DCHECK_EQ(load_ruleset_result_ == RulesetMatcher::kLoadSuccess, !!matcher_);
  return *load_ruleset_result_;
}

void RulesetInfo::CreateVerifiedMatcher() {
  DCHECK(expected_checksum_);
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  load_ruleset_result_ = RulesetMatcher::CreateVerifiedMatcher(
      source_, *expected_checksum_, &matcher_);

  UMA_HISTOGRAM_ENUMERATION(
      "Extensions.DeclarativeNetRequest.LoadRulesetResult",
      load_ruleset_result(), RulesetMatcher::kLoadResultMax);
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
  DCHECK(!load_data.rulesets.empty());

  bool success = true;
  for (auto& ruleset : load_data.rulesets) {
    ruleset.CreateVerifiedMatcher();
    success &= ruleset.did_load_successfully();
  }

  if (success) {
    // Set priority explicitly to avoid unwanted task priority inheritance.
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
        base::BindOnce(std::move(ui_callback), std::move(load_data)));
    return;
  }

  // Loading one or more rulesets failed. Re-index them.

  // Using a WeakPtr is safe since |reindex_callback| will be called on this
  // sequence itself.
  auto reindex_callback =
      base::BindOnce(&FileSequenceHelper::OnRulesetsReindexed,
                     weak_factory_.GetWeakPtr(), std::move(ui_callback));
  ReindexHelper::Start(std::move(load_data), std::move(reindex_callback));
}

void FileSequenceHelper::UpdateDynamicRules(
    LoadRequestData load_data,
    std::vector<int> rule_ids_to_remove,
    std::vector<api::declarative_net_request::Rule> rules_to_add,
    UpdateDynamicRulesUICallback ui_callback) const {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(1u, load_data.rulesets.size());

  RulesetInfo& dynamic_ruleset = load_data.rulesets[0];
  DCHECK(!dynamic_ruleset.expected_checksum());

  auto log_status_and_dispatch_callback = [&ui_callback, &load_data](
                                              base::Optional<std::string> error,
                                              UpdateDynamicRulesStatus status) {
    base::UmaHistogramEnumeration(kUpdateDynamicRulesStatusHistogram, status);

    // Set priority explicitly to avoid unwanted task priority inheritance.
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
        base::BindOnce(std::move(ui_callback), std::move(load_data),
                       std::move(error)));
  };

  int new_ruleset_checksum = -1;
  std::string error;
  UpdateDynamicRulesStatus status = UpdateDynamicRulesStatus::kSuccess;
  if (!UpdateAndIndexDynamicRules(
          dynamic_ruleset.source(), std::move(rule_ids_to_remove),
          std::move(rules_to_add), &new_ruleset_checksum, &error, &status)) {
    DCHECK(!error.empty());
    log_status_and_dispatch_callback(std::move(error), status);
    return;
  }

  DCHECK_EQ(UpdateDynamicRulesStatus::kSuccess, status);
  dynamic_ruleset.set_expected_checksum(new_ruleset_checksum);
  dynamic_ruleset.set_new_checksum(new_ruleset_checksum);
  dynamic_ruleset.CreateVerifiedMatcher();

  if (!dynamic_ruleset.did_load_successfully()) {
    status =
        GetStatusForLoadRulesetError(dynamic_ruleset.load_ruleset_result());
    log_status_and_dispatch_callback(kInternalErrorUpdatingDynamicRules,
                                     status);
    return;
  }

  // Success.
  log_status_and_dispatch_callback(base::nullopt, status);
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
  base::PostTask(
      FROM_HERE,
      {content::BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
      base::BindOnce(std::move(ui_callback), std::move(load_data)));
}

}  // namespace declarative_net_request
}  // namespace extensions
