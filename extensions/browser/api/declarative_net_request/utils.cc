// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/utils.h"

#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/flat_ruleset_indexer.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/service_manager/public/cpp/identity.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = extensions::api::declarative_net_request;

// The ruleset format version of the flatbuffer schema. Increment this whenever
// making an incompatible change to the schema at extension_ruleset.fbs or
// url_pattern_index.fbs. Whenever an extension with an indexed ruleset format
// version different from the one currently used by Chrome is loaded, the
// extension ruleset will be reindexed.
constexpr int kIndexedRulesetFormatVersion = 4;

// This static assert is meant to catch cases where
// url_pattern_index::kUrlPatternIndexFormatVersion is incremented without
// updating kIndexedRulesetFormatVersion.
static_assert(url_pattern_index::kUrlPatternIndexFormatVersion == 4,
              "kUrlPatternIndexFormatVersion has changed, make sure you've "
              "also updated kIndexedRulesetFormatVersion above.");

constexpr int kInvalidIndexedRulesetFormatVersion = -1;

int g_indexed_ruleset_format_version_for_testing =
    kInvalidIndexedRulesetFormatVersion;

int GetIndexedRulesetFormatVersion() {
  return g_indexed_ruleset_format_version_for_testing ==
                 kInvalidIndexedRulesetFormatVersion
             ? kIndexedRulesetFormatVersion
             : g_indexed_ruleset_format_version_for_testing;
}

// Returns the header to be used for indexed rulesets. This depends on the
// current ruleset format version.
std::string GetVersionHeader() {
  return base::StringPrintf("---------Version=%d",
                            GetIndexedRulesetFormatVersion());
}

// Returns the checksum of the given serialized |data|. |data| must not include
// the version header.
int GetChecksum(base::span<const uint8_t> data) {
  uint32_t hash = base::PersistentHash(data.data(), data.size());

  // Strip off the sign bit since this needs to be persisted in preferences
  // which don't support unsigned ints.
  return static_cast<int>(hash & 0x7fffffff);
}

// Helper function to persist the indexed ruleset |data| for |extension|. The
// ruleset is composed of a version header corresponding to the current ruleset
// format version, followed by the actual ruleset data. Note: The checksum only
// corresponds to this ruleset data and does not include the version header.
bool PersistRuleset(const Extension& extension,
                    base::span<const uint8_t> data,
                    int* ruleset_checksum) {
  DCHECK(ruleset_checksum);

  const base::FilePath path =
      file_util::GetIndexedRulesetPath(extension.path());

  // Create the directory corresponding to |path| if it does not exist.
  if (!base::CreateDirectory(path.DirName()))
    return false;

  base::File ruleset_file(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!ruleset_file.IsValid())
    return false;

  // Write the version header.
  std::string version_header = GetVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (ruleset_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return false;
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return false;
  int data_size = static_cast<int>(data.size());
  if (ruleset_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                     data_size) != data_size) {
    return false;
  }

  *ruleset_checksum = GetChecksum(data);
  return true;
}

// Helper to retrieve the ruleset ExtensionResource for |extension|.
const ExtensionResource* GetRulesetResource(const Extension& extension) {
  return declarative_net_request::DNRManifestData::GetRulesetResource(
      &extension);
}

// Helper to retrieve the filename of the JSON ruleset provided by |extension|.
std::string GetJSONRulesetFilename(const Extension& extension) {
  return GetRulesetResource(extension)->GetFilePath().BaseName().AsUTF8Unsafe();
}

InstallWarning CreateInstallWarning(const std::string& message) {
  return InstallWarning(message, manifest_keys::kDeclarativeNetRequestKey,
                        manifest_keys::kDeclarativeRuleResourcesKey);
}

// Helper function to index |rules| and persist them to the
// |indexed_ruleset_path|.
ParseInfo IndexAndPersistRulesImpl(const base::Value& rules,
                                   const Extension& extension,
                                   std::vector<InstallWarning>* warnings,
                                   int* ruleset_checksum) {
  DCHECK(warnings);
  DCHECK(ruleset_checksum);

  if (!rules.is_list())
    return ParseInfo(ParseResult::ERROR_LIST_NOT_PASSED);

  FlatRulesetIndexer indexer;

  const size_t kRuleCountLimit = dnr_api::MAX_NUMBER_OF_RULES;
  bool rule_count_exceeded = false;

  // Limit the maximum number of rule unparsed warnings to 5.
  const size_t kMaxUnparsedRulesWarnings = 5;
  std::vector<int> unparsed_indices;
  unparsed_indices.reserve(kMaxUnparsedRulesWarnings);
  bool unparsed_warnings_limit_exeeded = false;

  base::ElapsedTimer timer;
  {
    std::set<int> id_set;  // Ensure all ids are distinct.
    std::unique_ptr<dnr_api::Rule> parsed_rule;

    const auto& rules_list = rules.GetList();
    for (size_t i = 0; i < rules_list.size(); i++) {
      parsed_rule = dnr_api::Rule::FromValue(rules_list[i]);

      // Ignore rules which can't be successfully parsed and show an install
      // warning for them. A hard error is not thrown to maintain backwards
      // compatibility.
      if (!parsed_rule) {
        if (unparsed_indices.size() < kMaxUnparsedRulesWarnings)
          unparsed_indices.push_back(i);
        else
          unparsed_warnings_limit_exeeded = true;
        continue;
      }

      bool inserted = id_set.insert(parsed_rule->id).second;
      if (!inserted)
        return ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, i);

      IndexedRule indexed_rule;
      ParseResult parse_result =
          IndexedRule::CreateIndexedRule(std::move(parsed_rule), &indexed_rule);
      if (parse_result != ParseResult::SUCCESS)
        return ParseInfo(parse_result, i);

      if (indexer.indexed_rules_count() >= kRuleCountLimit) {
        rule_count_exceeded = true;
        break;
      }

      indexer.AddUrlRule(indexed_rule);
    }
  }
  indexer.Finish();
  UMA_HISTOGRAM_TIMES(kIndexRulesTimeHistogram, timer.Elapsed());

  if (!PersistRuleset(extension, indexer.GetData(), ruleset_checksum))
    return ParseInfo(ParseResult::ERROR_PERSISTING_RULESET);

  if (rule_count_exceeded)
    warnings->push_back(CreateInstallWarning(kRuleCountExceeded));

  if (unparsed_warnings_limit_exeeded) {
    DCHECK_EQ(kMaxUnparsedRulesWarnings, unparsed_indices.size());
    warnings->push_back(CreateInstallWarning(ErrorUtils::FormatErrorMessage(
        kTooManyParseFailuresWarning,
        std::to_string(kMaxUnparsedRulesWarnings))));
  }

  for (int rule_index : unparsed_indices) {
    warnings->push_back(CreateInstallWarning(ErrorUtils::FormatErrorMessage(
        kRuleNotParsedWarning, std::to_string(rule_index))));
  }

  UMA_HISTOGRAM_TIMES(kIndexAndPersistRulesTimeHistogram, timer.Elapsed());
  UMA_HISTOGRAM_COUNTS_100000(kManifestRulesCountHistogram,
                              indexer.indexed_rules_count());

  return ParseInfo(ParseResult::SUCCESS);
}

void OnSafeJSONParserSuccess(const Extension* extension,
                             IndexAndPersistRulesCallback callback,
                             std::unique_ptr<base::Value> root) {
  DCHECK(root);

  std::vector<InstallWarning> warnings;
  int ruleset_checksum;
  const ParseInfo info =
      IndexAndPersistRulesImpl(*root, *extension, &warnings, &ruleset_checksum);
  if (info.result() == ParseResult::SUCCESS) {
    std::move(callback).Run(IndexAndPersistRulesResult::CreateSuccessResult(
        ruleset_checksum, std::move(warnings)));
    return;
  }

  std::string error =
      info.GetErrorDescription(GetJSONRulesetFilename(*extension));
  std::move(callback).Run(
      IndexAndPersistRulesResult::CreateErrorResult(std::move(error)));
}

std::string GetJSONParseError(const std::string& json_ruleset_filename,
                              const std::string& json_parse_error) {
  return base::StrCat({json_ruleset_filename, ": ", json_parse_error});
}

void OnSafeJSONParserError(IndexAndPersistRulesCallback callback,
                           const std::string& json_ruleset_filename,
                           const std::string& json_parse_error) {
  std::move(callback).Run(IndexAndPersistRulesResult::CreateErrorResult(
      GetJSONParseError(json_ruleset_filename, json_parse_error)));
}

}  // namespace

// static
IndexAndPersistRulesResult IndexAndPersistRulesResult::CreateSuccessResult(
    int ruleset_checksum,
    std::vector<InstallWarning> warnings) {
  IndexAndPersistRulesResult result;
  result.success = true;
  result.ruleset_checksum = ruleset_checksum;
  result.warnings = std::move(warnings);
  return result;
}

// static
IndexAndPersistRulesResult IndexAndPersistRulesResult::CreateErrorResult(
    std::string error) {
  IndexAndPersistRulesResult result;
  result.success = false;
  result.error = std::move(error);
  return result;
}

IndexAndPersistRulesResult::~IndexAndPersistRulesResult() = default;
IndexAndPersistRulesResult::IndexAndPersistRulesResult(
    IndexAndPersistRulesResult&&) = default;
IndexAndPersistRulesResult& IndexAndPersistRulesResult::operator=(
    IndexAndPersistRulesResult&&) = default;
IndexAndPersistRulesResult::IndexAndPersistRulesResult() = default;

IndexAndPersistRulesResult IndexAndPersistRulesUnsafe(
    const Extension& extension) {
  DCHECK(IsAPIAvailable());

  const ExtensionResource* resource = GetRulesetResource(extension);
  DCHECK(resource);

  JSONFileValueDeserializer deserializer(resource->GetFilePath());
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(
      nullptr /*error_code*/, &error /*error_message*/);
  if (!root) {
    return IndexAndPersistRulesResult::CreateErrorResult(
        GetJSONParseError(GetJSONRulesetFilename(extension), error));
  }

  std::vector<InstallWarning> warnings;
  int ruleset_checksum;
  const ParseInfo info =
      IndexAndPersistRulesImpl(*root, extension, &warnings, &ruleset_checksum);
  if (info.result() == ParseResult::SUCCESS) {
    return IndexAndPersistRulesResult::CreateSuccessResult(ruleset_checksum,
                                                           std::move(warnings));
  }

  error = info.GetErrorDescription(GetJSONRulesetFilename(extension));
  return IndexAndPersistRulesResult::CreateErrorResult(std::move(error));
}

void IndexAndPersistRules(service_manager::Connector* connector,
                          service_manager::Identity* identity,
                          const Extension& extension,
                          IndexAndPersistRulesCallback callback) {
  DCHECK(IsAPIAvailable());

  const ExtensionResource* resource = GetRulesetResource(extension);
  DCHECK(resource);

  std::string json_contents;
  if (!base::ReadFileToString(resource->GetFilePath(), &json_contents)) {
    std::move(callback).Run(IndexAndPersistRulesResult::CreateErrorResult(
        manifest_errors::kDeclarativeNetRequestJSONRulesFileReadError));
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  auto success_callback =
      base::BindRepeating(&OnSafeJSONParserSuccess,
                          base::RetainedRef(&extension), repeating_callback);
  auto error_callback =
      base::BindRepeating(&OnSafeJSONParserError, repeating_callback,
                          GetJSONRulesetFilename(extension));

  if (identity) {
    data_decoder::SafeJsonParser::ParseBatch(connector, json_contents,
                                             success_callback, error_callback,
                                             identity->instance());
  } else {
    data_decoder::SafeJsonParser::Parse(connector, json_contents,
                                        success_callback, error_callback);
  }
}

bool IsValidRulesetData(base::span<const uint8_t> data, int expected_checksum) {
  flatbuffers::Verifier verifier(data.data(), data.size());
  return expected_checksum == GetChecksum(data) &&
         flat::VerifyExtensionIndexedRulesetBuffer(verifier);
}

std::string GetVersionHeaderForTesting() {
  return GetVersionHeader();
}

int GetIndexedRulesetFormatVersionForTesting() {
  return GetIndexedRulesetFormatVersion();
}

void SetIndexedRulesetFormatVersionForTesting(int version) {
  DCHECK_NE(kInvalidIndexedRulesetFormatVersion, version);
  g_indexed_ruleset_format_version_for_testing = version;
}

bool StripVersionHeaderAndParseVersion(std::string* ruleset_data) {
  DCHECK(ruleset_data);
  const std::string version_header = GetVersionHeader();

  if (!base::StartsWith(*ruleset_data, version_header,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  // Strip the header from |ruleset_data|.
  ruleset_data->erase(0, version_header.size());
  return true;
}

}  // namespace declarative_net_request
}  // namespace extensions
