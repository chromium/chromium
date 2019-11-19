// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_source.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "tools/json_schema_compiler/util.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;
using Status = ReadJSONRulesResult::Status;

// Dynamic rulesets get more priority over static rulesets.
const size_t kStaticRulesetPriority = 1;
const size_t kDynamicRulesetPriority = 2;

constexpr const char kFileDoesNotExistError[] = "File does not exist.";
constexpr const char kFileReadError[] = "File read error.";

constexpr const char kDynamicRulesetDirectory[] = "DNR Extension Rules";
constexpr const char kDynamicRulesJSONFilename[] = "rules.json";
constexpr const char kDynamicIndexedRulesFilename[] = "rules.fbs";

// Helper to retrieve the filename for the given |file_path|.
std::string GetFilename(const base::FilePath& file_path) {
  return file_path.BaseName().AsUTF8Unsafe();
}

std::string GetErrorWithFilename(const base::FilePath& path,
                                 const std::string& error) {
  return base::StrCat({GetFilename(path), ": ", error});
}

InstallWarning CreateInstallWarning(const std::string& message) {
  return InstallWarning(message, manifest_keys::kDeclarativeNetRequestKey,
                        manifest_keys::kDeclarativeRuleResourcesKey);
}

ReadJSONRulesResult ParseRulesFromJSON(const base::Value& rules,
                                       size_t rule_limit) {
  ReadJSONRulesResult result;

  if (!rules.is_list()) {
    return ReadJSONRulesResult::CreateErrorResult(Status::kJSONIsNotList,
                                                  kErrorListNotPassed);
  }

  // Limit the maximum number of rule unparsed warnings to 5.
  const size_t kMaxUnparsedRulesWarnings = 5;
  bool unparsed_warnings_limit_exeeded = false;
  size_t unparsed_warning_count = 0;

  // We don't use json_schema_compiler::util::PopulateArrayFromList since it
  // fails if a single Value can't be deserialized. However we want to ignore
  // values which can't be parsed to maintain backwards compatibility.
  const auto& rules_list = rules.GetList();
  for (size_t i = 0; i < rules_list.size(); i++) {
    dnr_api::Rule parsed_rule;
    base::string16 parse_error;

    if (dnr_api::Rule::Populate(rules_list[i], &parsed_rule, &parse_error) &&
        parse_error.empty()) {
      if (result.rules.size() == rule_limit) {
        result.rule_parse_warnings.push_back(
            CreateInstallWarning(kRuleCountExceeded));
        break;
      }

      result.rules.push_back(std::move(parsed_rule));
      continue;
    }

    if (unparsed_warning_count == kMaxUnparsedRulesWarnings) {
      // Don't add the warning for the current rule.
      unparsed_warnings_limit_exeeded = true;
      continue;
    }

    ++unparsed_warning_count;
    std::string rule_location;

    // If possible use the rule ID in the install warning.
    if (auto* id_val =
            rules_list[i].FindKeyOfType(kIDKey, base::Value::Type::INTEGER)) {
      rule_location = base::StringPrintf("id %d", id_val->GetInt());
    } else {
      // Use one-based indices.
      rule_location = base::StringPrintf("index %zu", i + 1);
    }

    result.rule_parse_warnings.push_back(CreateInstallWarning(
        ErrorUtils::FormatErrorMessage(kRuleNotParsedWarning, rule_location,
                                       base::UTF16ToUTF8(parse_error))));
  }

  DCHECK_LE(result.rules.size(), rule_limit);

  if (unparsed_warnings_limit_exeeded) {
    result.rule_parse_warnings.push_back(
        CreateInstallWarning(ErrorUtils::FormatErrorMessage(
            kTooManyParseFailuresWarning,
            std::to_string(kMaxUnparsedRulesWarnings))));
  }

  return result;
}

void OnSafeJSONParse(const base::FilePath& json_path,
                     const RulesetSource& source,
                     RulesetSource::IndexAndPersistJSONRulesetCallback callback,
                     data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    std::move(callback).Run(IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(json_path, *result.error)));
    return;
  }

  base::ElapsedTimer timer;
  ReadJSONRulesResult read_result =
      ParseRulesFromJSON(*result.value, source.rule_count_limit());
  if (read_result.status != Status::kSuccess) {
    std::move(callback).Run(IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(source.json_path(), read_result.error)));
    return;
  }

  int ruleset_checksum;
  size_t rules_count = read_result.rules.size();
  const ParseInfo info = source.IndexAndPersistRules(
      std::move(read_result.rules), &ruleset_checksum);
  if (info.result() == ParseResult::SUCCESS) {
    std::move(callback).Run(
        IndexAndPersistJSONRulesetResult::CreateSuccessResult(
            ruleset_checksum, std::move(read_result.rule_parse_warnings),
            rules_count, timer.Elapsed()));
    return;
  }

  std::string error =
      GetErrorWithFilename(source.json_path(), info.GetErrorDescription());
  std::move(callback).Run(
      IndexAndPersistJSONRulesetResult::CreateErrorResult(std::move(error)));
}

}  // namespace

// static
IndexAndPersistJSONRulesetResult
IndexAndPersistJSONRulesetResult::CreateSuccessResult(
    int ruleset_checksum,
    std::vector<InstallWarning> warnings,
    size_t rules_count,
    base::TimeDelta index_and_persist_time) {
  IndexAndPersistJSONRulesetResult result;
  result.success = true;
  result.ruleset_checksum = ruleset_checksum;
  result.warnings = std::move(warnings);
  result.rules_count = rules_count;
  result.index_and_persist_time = index_and_persist_time;
  return result;
}

// static
IndexAndPersistJSONRulesetResult
IndexAndPersistJSONRulesetResult::CreateErrorResult(std::string error) {
  IndexAndPersistJSONRulesetResult result;
  result.success = false;
  result.error = std::move(error);
  return result;
}

IndexAndPersistJSONRulesetResult::~IndexAndPersistJSONRulesetResult() = default;
IndexAndPersistJSONRulesetResult::IndexAndPersistJSONRulesetResult(
    IndexAndPersistJSONRulesetResult&&) = default;
IndexAndPersistJSONRulesetResult& IndexAndPersistJSONRulesetResult::operator=(
    IndexAndPersistJSONRulesetResult&&) = default;
IndexAndPersistJSONRulesetResult::IndexAndPersistJSONRulesetResult() = default;

// static
ReadJSONRulesResult ReadJSONRulesResult::CreateErrorResult(Status status,
                                                           std::string error) {
  ReadJSONRulesResult result;
  result.status = status;
  result.error = std::move(error);
  return result;
}

ReadJSONRulesResult::ReadJSONRulesResult() = default;
ReadJSONRulesResult::~ReadJSONRulesResult() = default;
ReadJSONRulesResult::ReadJSONRulesResult(ReadJSONRulesResult&&) = default;
ReadJSONRulesResult& ReadJSONRulesResult::operator=(ReadJSONRulesResult&&) =
    default;

// static
const size_t RulesetSource::kStaticRulesetID = 1;
const size_t RulesetSource::kDynamicRulesetID = 2;

// static
RulesetSource RulesetSource::CreateStatic(const Extension& extension) {
  return RulesetSource(
      declarative_net_request::DNRManifestData::GetRulesetPath(extension),
      file_util::GetIndexedRulesetPath(extension.path()), kStaticRulesetID,
      kStaticRulesetPriority, dnr_api::SOURCE_TYPE_MANIFEST,
      dnr_api::MAX_NUMBER_OF_RULES, extension.id());
}

// static
RulesetSource RulesetSource::CreateDynamic(content::BrowserContext* context,
                                           const Extension& extension) {
  base::FilePath dynamic_ruleset_directory =
      context->GetPath()
          .AppendASCII(kDynamicRulesetDirectory)
          .AppendASCII(extension.id());
  return RulesetSource(
      dynamic_ruleset_directory.AppendASCII(kDynamicRulesJSONFilename),
      dynamic_ruleset_directory.AppendASCII(kDynamicIndexedRulesFilename),
      kDynamicRulesetID, kDynamicRulesetPriority, dnr_api::SOURCE_TYPE_DYNAMIC,
      dnr_api::MAX_NUMBER_OF_DYNAMIC_RULES, extension.id());
}

// static
std::unique_ptr<RulesetSource> RulesetSource::CreateTemporarySource(
    size_t id,
    size_t priority,
    dnr_api::SourceType type,
    size_t rule_count_limit,
    ExtensionId extension_id) {
  base::FilePath temporary_file_indexed;
  base::FilePath temporary_file_json;
  if (!base::CreateTemporaryFile(&temporary_file_indexed) ||
      !base::CreateTemporaryFile(&temporary_file_json)) {
    return nullptr;
  }

  return std::make_unique<RulesetSource>(
      std::move(temporary_file_json), std::move(temporary_file_indexed), id,
      priority, type, rule_count_limit, std::move(extension_id));
}

RulesetSource::RulesetSource(base::FilePath json_path,
                             base::FilePath indexed_path,
                             size_t id,
                             size_t priority,
                             dnr_api::SourceType type,
                             size_t rule_count_limit,
                             ExtensionId extension_id)
    : json_path_(std::move(json_path)),
      indexed_path_(std::move(indexed_path)),
      id_(id),
      priority_(priority),
      type_(type),
      rule_count_limit_(rule_count_limit),
      extension_id_(std::move(extension_id)) {}

RulesetSource::~RulesetSource() = default;
RulesetSource::RulesetSource(RulesetSource&&) = default;
RulesetSource& RulesetSource::operator=(RulesetSource&&) = default;

RulesetSource RulesetSource::Clone() const {
  return RulesetSource(json_path_, indexed_path_, id_, priority_, type_,
                       rule_count_limit_, extension_id_);
}

IndexAndPersistJSONRulesetResult
RulesetSource::IndexAndPersistJSONRulesetUnsafe() const {
  DCHECK(IsAPIAvailable());

  base::ElapsedTimer timer;
  ReadJSONRulesResult result = ReadJSONRulesUnsafe();
  if (result.status != Status::kSuccess) {
    return IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(json_path_, result.error));
  }

  int ruleset_checksum;
  size_t rules_count = result.rules.size();
  const ParseInfo info =
      IndexAndPersistRules(std::move(result.rules), &ruleset_checksum);
  if (info.result() == ParseResult::SUCCESS) {
    return IndexAndPersistJSONRulesetResult::CreateSuccessResult(
        ruleset_checksum, std::move(result.rule_parse_warnings), rules_count,
        timer.Elapsed());
  }

  std::string error =
      GetErrorWithFilename(json_path_, info.GetErrorDescription());
  return IndexAndPersistJSONRulesetResult::CreateErrorResult(std::move(error));
}

void RulesetSource::IndexAndPersistJSONRuleset(
    data_decoder::DataDecoder* decoder,
    IndexAndPersistJSONRulesetCallback callback) const {
  DCHECK(IsAPIAvailable());

  if (!base::PathExists(json_path_)) {
    std::move(callback).Run(IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(json_path_, kFileDoesNotExistError)));
    return;
  }

  std::string json_contents;
  if (!base::ReadFileToString(json_path_, &json_contents)) {
    std::move(callback).Run(IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(json_path_, kFileReadError)));
    return;
  }

  decoder->ParseJson(json_contents,
                     base::BindOnce(&OnSafeJSONParse, json_path_, Clone(),
                                    std::move(callback)));
}

ParseInfo RulesetSource::IndexAndPersistRules(std::vector<dnr_api::Rule> rules,
                                              int* ruleset_checksum) const {
  DCHECK_LE(rules.size(), rule_count_limit_);
  DCHECK(ruleset_checksum);
  DCHECK(IsAPIAvailable());

  FlatRulesetIndexer indexer;

  {
    std::set<int> id_set;  // Ensure all ids are distinct.
    const GURL base_url = Extension::GetBaseURLFromExtensionId(extension_id_);
    for (auto& rule : rules) {
      int rule_id = rule.id;
      bool inserted = id_set.insert(rule_id).second;
      if (!inserted)
        return ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, rule_id);

      IndexedRule indexed_rule;
      ParseResult parse_result = IndexedRule::CreateIndexedRule(
          std::move(rule), base_url, &indexed_rule);
      if (parse_result != ParseResult::SUCCESS)
        return ParseInfo(parse_result, rule_id);

      indexer.AddUrlRule(indexed_rule);
    }
  }
  indexer.Finish();

  if (!PersistIndexedRuleset(indexed_path_, indexer.GetData(),
                             ruleset_checksum)) {
    return ParseInfo(ParseResult::ERROR_PERSISTING_RULESET);
  }

  return ParseInfo(ParseResult::SUCCESS);
}

ReadJSONRulesResult RulesetSource::ReadJSONRulesUnsafe() const {
  ReadJSONRulesResult result;

  if (!base::PathExists(json_path_)) {
    return ReadJSONRulesResult::CreateErrorResult(Status::kFileDoesNotExist,
                                                  kFileDoesNotExistError);
  }

  std::string json_contents;
  if (!base::ReadFileToString(json_path_, &json_contents)) {
    return ReadJSONRulesResult::CreateErrorResult(Status::kFileReadError,
                                                  kFileReadError);
  }

  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(
          json_contents, base::JSON_PARSE_RFC /* options */);
  if (!value_with_error.value) {
    return ReadJSONRulesResult::CreateErrorResult(
        Status::kJSONParseError, std::move(value_with_error.error_message));
  }

  return ParseRulesFromJSON(*value_with_error.value, rule_count_limit_);
}

bool RulesetSource::WriteRulesToJSON(
    const std::vector<dnr_api::Rule>& rules) const {
  DCHECK_LE(rules.size(), rule_count_limit_);

  std::unique_ptr<base::Value> rules_value =
      json_schema_compiler::util::CreateValueFromArray(rules);
  DCHECK(rules_value);
  DCHECK(rules_value->is_list());

  if (!base::CreateDirectory(json_path_.DirName()))
    return false;

  std::string json_contents;
  JSONStringValueSerializer serializer(&json_contents);
  serializer.set_pretty_print(false);
  if (!serializer.Serialize(*rules_value))
    return false;

  int data_size = static_cast<int>(json_contents.size());
  return base::WriteFile(json_path_, json_contents.data(), data_size) ==
         data_size;
}

}  // namespace declarative_net_request
}  // namespace extensions
