// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"

#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "tools/json_schema_compiler/util.h"

namespace extensions::declarative_net_request {

namespace {

namespace dnr_api = extensions::api::declarative_net_request;
using Status = ReadJSONRulesResult::Status;

constexpr const char kFileDoesNotExistError[] = "File does not exist.";
constexpr const char kFileReadError[] = "File read error.";

constexpr const char kDynamicRulesetDirectory[] = "DNR Extension Rules";
constexpr const char kDynamicRulesJSONFilename[] = "rules.json";
constexpr const char kDynamicIndexedRulesFilename[] = "rules.fbs";

// Helper to retrieve the filename for the given |file_path|.
std::string GetFilename(const base::FilePath& file_path) {
  return file_path.BaseName().AsUTF8Unsafe();
}

std::string GetErrorWithFilename(const base::FilePath& json_path,
                                 std::string_view error) {
  return base::StrCat({GetFilename(json_path), ": ", error});
}

InstallWarning CreateInstallWarning(const base::FilePath& json_path,
                                    const std::string& message) {
  std::string message_with_filename = GetErrorWithFilename(json_path, message);
  return InstallWarning(message_with_filename,
                        dnr_api::ManifestKeys::kDeclarativeNetRequest,
                        dnr_api::DNRInfo::kRuleResources);
}

ReadJSONRulesResult ParseRulesFromJSON(const RulesetID& ruleset_id,
                                       const base::FilePath& json_path,
                                       const base::Value& rules,
                                       size_t rule_limit,
                                       bool is_dynamic_ruleset) {
  ReadJSONRulesResult result;

  if (!rules.is_list()) {
    return ReadJSONRulesResult::CreateErrorResult(Status::kJSONIsNotList,
                                                  kErrorListNotPassed);
  }

  int regex_rule_count = 0;
  bool regex_rule_count_exceeded = false;

  // We don't use json_schema_compiler::util::PopulateArrayFromList since it
  // fails if a single Value can't be deserialized. However we want to ignore
  // values which can't be parsed to maintain backwards compatibility.
  const base::Value::List& rules_list = rules.GetList();

  // Ignore any rulesets which exceed the static rule count limit (This is
  // defined as dnr_api::GUARANTEED_MINIMUM_STATIC_RULES + the global rule count
  // limit). We do this because such a ruleset can never be enabled in its
  // entirety.
  if (rules_list.size() > rule_limit && !is_dynamic_ruleset) {
    result.status = ReadJSONRulesResult::Status::kRuleCountLimitExceeded;
    result.error = ErrorUtils::FormatErrorMessage(
        kIndexingRuleLimitExceeded, base::NumberToString(ruleset_id.value()));

    return result;
  }

  for (size_t i = 0; i < rules_list.size(); i++) {
    auto parsed_rule = dnr_api::Rule::FromValue(rules_list[i]);
    if (parsed_rule.has_value()) {
      if (result.rules.size() == rule_limit) {
        result.rule_parse_warnings.push_back(
            CreateInstallWarning(json_path, kRuleCountExceeded));
        break;
      }

      const bool is_regex_rule = !!parsed_rule->condition.regex_filter;
      if (is_regex_rule && ++regex_rule_count > GetRegexRuleLimit()) {
        // Only add the install warning once.
        if (!regex_rule_count_exceeded) {
          regex_rule_count_exceeded = true;
          result.rule_parse_warnings.push_back(
              CreateInstallWarning(json_path, kRegexRuleCountExceeded));
        }

        continue;
      }

      result.rules.push_back(std::move(*parsed_rule));
      continue;
    }

    std::string rule_location;

    // If possible use the rule ID in the install warning.
    if (auto id = rules_list[i].GetDict().FindInt(kIDKey)) {
      rule_location = base::StringPrintf("id %d", *id);
    } else {
      // Use one-based indices.
      rule_location = base::StringPrintf("index %zu", i + 1);
    }

    result.rule_parse_warnings.push_back(CreateInstallWarning(
        json_path, ErrorUtils::FormatErrorMessage(
                       kRuleNotParsedWarning, rule_location,
                       base::UTF16ToUTF8(parsed_rule.error()))));
  }

  DCHECK_LE(result.rules.size(), rule_limit);

  return result;
}

IndexAndPersistJSONRulesetResult IndexAndPersistRuleset(
    const FileBackedRulesetSource& source,
    ReadJSONRulesResult read_result,
    const base::ElapsedTimer& timer,
    uint8_t parse_flags) {
  // Rulesets which exceed the rule limit are ignored because they can never be
  // enabled without breaking the limit.
  if (read_result.status == Status::kRuleCountLimitExceeded) {
    std::vector<InstallWarning> warnings;
    warnings.push_back(
        CreateInstallWarning(source.json_path(), read_result.error));

    return IndexAndPersistJSONRulesetResult::CreateIgnoreResult(
        std::move(warnings));
  } else if (read_result.status != Status::kSuccess) {
    return IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(source.json_path(), read_result.error));
  }

  DCHECK_EQ(Status::kSuccess, read_result.status);

  const ParseInfo info =
      source.IndexRules(std::move(read_result.rules), parse_flags);

  if (info.has_error()) {
    return IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(source.json_path(), info.error()));
  }

  if (!PersistIndexedRuleset(source.indexed_path(), info.GetBuffer())) {
    return IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(source.json_path(), kErrorPersisting));
  }

  // Parsing errors (e.g. rule ID of "invalid") are always considered to be
  // install warnings. This helps ensure backwards compatibility as the rule
  // schema is changed.
  std::vector<InstallWarning> warnings =
      std::move(read_result.rule_parse_warnings);

  for (const auto& warning : info.rule_ignored_warnings()) {
    warnings.push_back(
        CreateInstallWarning(source.json_path(), warning.message));
  }

  // Limit the maximum number of rule parsing warnings to 5.
  const size_t kMaxUnparsedRulesWarnings = 5;
  if (warnings.size() > kMaxUnparsedRulesWarnings) {
    warnings.erase(warnings.begin() + kMaxUnparsedRulesWarnings,
                   warnings.end());
    warnings.push_back(CreateInstallWarning(
        source.json_path(),
        ErrorUtils::FormatErrorMessage(
            kTooManyParseFailuresWarning,
            base::NumberToString(kMaxUnparsedRulesWarnings))));
  }

  return IndexAndPersistJSONRulesetResult::CreateSuccessResult(
      info.ruleset_checksum(), std::move(warnings), info.rules_count(),
      info.regex_rules_count(), timer.Elapsed());
}

void OnSafeJSONParse(
    const base::FilePath& json_path,
    const FileBackedRulesetSource& source,
    uint8_t parse_flags,
    FileBackedRulesetSource::IndexAndPersistJSONRulesetCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(IndexAndPersistJSONRulesetResult::CreateErrorResult(
        GetErrorWithFilename(json_path, result.error())));
    return;
  }

  base::ElapsedTimer timer;
  ReadJSONRulesResult read_result = ParseRulesFromJSON(
      source.id(), json_path, *result, source.rule_count_limit(),
      source.is_dynamic_ruleset());

  std::move(callback).Run(IndexAndPersistRuleset(source, std::move(read_result),
                                                 timer, parse_flags));
}

}  // namespace

// static
IndexAndPersistJSONRulesetResult
IndexAndPersistJSONRulesetResult::CreateSuccessResult(
    int ruleset_checksum,
    std::vector<InstallWarning> warnings,
    size_t rules_count,
    size_t regex_rules_count,
    base::TimeDelta index_and_persist_time) {
  IndexAndPersistJSONRulesetResult result;
  result.status = IndexAndPersistJSONRulesetResult::Status::kSuccess;
  result.ruleset_checksum = ruleset_checksum;
  result.warnings = std::move(warnings);
  result.rules_count = rules_count;
  result.regex_rules_count = regex_rules_count;
  result.index_and_persist_time = index_and_persist_time;
  return result;
}

// static
IndexAndPersistJSONRulesetResult
IndexAndPersistJSONRulesetResult::CreateIgnoreResult(
    std::vector<InstallWarning> warnings) {
  IndexAndPersistJSONRulesetResult result;
  result.status = IndexAndPersistJSONRulesetResult::Status::kIgnore;
  result.warnings = std::move(warnings);
  return result;
}

// static
IndexAndPersistJSONRulesetResult
IndexAndPersistJSONRulesetResult::CreateErrorResult(std::string error) {
  IndexAndPersistJSONRulesetResult result;
  result.status = IndexAndPersistJSONRulesetResult::Status::kError;
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
std::vector<FileBackedRulesetSource> FileBackedRulesetSource::CreateStatic(
    const Extension& extension,
    RulesetFilter ruleset_filter) {
  const std::vector<DNRManifestData::RulesetInfo>& rulesets =
      declarative_net_request::DNRManifestData::GetRulesets(extension);

  bool only_enabled = ruleset_filter == RulesetFilter::kIncludeManifestEnabled;

  std::vector<FileBackedRulesetSource> sources;
  for (const auto& info : rulesets) {
    if (!only_enabled || info.enabled) {
      sources.push_back(CreateStatic(extension, info));
    }
  }

  return sources;
}

FileBackedRulesetSource FileBackedRulesetSource::CreateStatic(
    const Extension& extension,
    const DNRManifestData::RulesetInfo& info) {
  return FileBackedRulesetSource(
      extension.path().Append(info.relative_path),
      extension.path().Append(
          file_util::GetIndexedRulesetRelativePath(info.id.value())),
      info.id, GetMaximumRulesPerRuleset(), extension.id(), info.enabled);
}

// static
FileBackedRulesetSource FileBackedRulesetSource::CreateDynamic(
    content::BrowserContext* context,
    const ExtensionId& extension_id) {
  base::FilePath dynamic_ruleset_directory =
      context->GetPath()
          .AppendASCII(kDynamicRulesetDirectory)
          .AppendASCII(extension_id);
  return FileBackedRulesetSource(
      dynamic_ruleset_directory.AppendASCII(kDynamicRulesJSONFilename),
      dynamic_ruleset_directory.AppendASCII(kDynamicIndexedRulesFilename),
      kDynamicRulesetID, GetDynamicRuleLimit(), extension_id,
      true /* enabled_by_default */);
}

// static
std::unique_ptr<FileBackedRulesetSource>
FileBackedRulesetSource::CreateTemporarySource(RulesetID id,
                                               size_t rule_count_limit,
                                               ExtensionId extension_id) {
  base::FilePath temporary_file_indexed;
  base::FilePath temporary_file_json;
  if (!base::CreateTemporaryFile(&temporary_file_indexed) ||
      !base::CreateTemporaryFile(&temporary_file_json)) {
    return nullptr;
  }

  // Use WrapUnique since FileBackedRulesetSource constructor is private.
  return base::WrapUnique(new FileBackedRulesetSource(
      std::move(temporary_file_json), std::move(temporary_file_indexed), id,
      rule_count_limit, std::move(extension_id),
      true /* enabled_by_default */));
}

FileBackedRulesetSource::~FileBackedRulesetSource() = default;
FileBackedRulesetSource::FileBackedRulesetSource(FileBackedRulesetSource&&) =
    default;
FileBackedRulesetSource& FileBackedRulesetSource::operator=(
    FileBackedRulesetSource&&) = default;

FileBackedRulesetSource FileBackedRulesetSource::Clone() const {
  return FileBackedRulesetSource(json_path_, indexed_path_, id(),
                                 rule_count_limit(), extension_id(),
                                 enabled_by_default());
}

IndexAndPersistJSONRulesetResult
FileBackedRulesetSource::IndexAndPersistJSONRulesetUnsafe(
    uint8_t parse_flags) const {
  base::ElapsedTimer timer;
  return IndexAndPersistRuleset(*this, ReadJSONRulesUnsafe(), timer,
                                parse_flags);
}

void FileBackedRulesetSource::IndexAndPersistJSONRuleset(
    data_decoder::DataDecoder* decoder,
    uint8_t parse_flags,
    IndexAndPersistJSONRulesetCallback callback) const {
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
                                    parse_flags, std::move(callback)));
}

ReadJSONRulesResult FileBackedRulesetSource::ReadJSONRulesUnsafe() const {
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

  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      json_contents, base::JSON_PARSE_RFC /* options */);
  if (!value_with_error.has_value()) {
    return ReadJSONRulesResult::CreateErrorResult(
        Status::kJSONParseError, std::move(value_with_error.error().message));
  }

  return ParseRulesFromJSON(id(), json_path(), *value_with_error,
                            rule_count_limit(), is_dynamic_ruleset());
}

bool FileBackedRulesetSource::SerializeRulesToJSON(
    const std::vector<dnr_api::Rule>& rules,
    std::string* json) const {
  DCHECK_LE(rules.size(), rule_count_limit());

  base::Value::List rules_value =
      json_schema_compiler::util::CreateValueFromArray(rules);

  JSONStringValueSerializer serializer(json);
  serializer.set_pretty_print(false);
  return serializer.Serialize(rules_value);
}

LoadRulesetResult FileBackedRulesetSource::CreateVerifiedMatcher(
    int expected_ruleset_checksum,
    std::unique_ptr<RulesetMatcher>* matcher) const {
  DCHECK(matcher);

  base::ElapsedTimer timer;

  if (!base::PathExists(indexed_path())) {
    return LoadRulesetResult::kErrorInvalidPath;
  }

  std::string ruleset_data;
  if (!base::ReadFileToString(indexed_path(), &ruleset_data)) {
    return LoadRulesetResult::kErrorCannotReadFile;
  }

  if (!StripVersionHeaderAndParseVersion(&ruleset_data)) {
    return LoadRulesetResult::kErrorVersionMismatch;
  }

  if (expected_ruleset_checksum !=
      GetChecksum(base::as_byte_span(ruleset_data))) {
    return LoadRulesetResult::kErrorChecksumMismatch;
  }

  LoadRulesetResult result =
      RulesetSource::CreateVerifiedMatcher(std::move(ruleset_data), matcher);
  if (result == LoadRulesetResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Extensions.DeclarativeNetRequest.CreateVerifiedMatcherTime",
        timer.Elapsed());
  }
  return result;
}

FileBackedRulesetSource::FileBackedRulesetSource(base::FilePath json_path,
                                                 base::FilePath indexed_path,
                                                 RulesetID id,
                                                 size_t rule_count_limit,
                                                 ExtensionId extension_id,
                                                 bool enabled_by_default)
    : RulesetSource(id,
                    rule_count_limit,
                    std::move(extension_id),
                    enabled_by_default),
      json_path_(std::move(json_path)),
      indexed_path_(std::move(indexed_path)) {}

}  // namespace extensions::declarative_net_request
