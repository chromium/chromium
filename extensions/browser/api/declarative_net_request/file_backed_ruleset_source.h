// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_BACKED_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_BACKED_RULESET_SOURCE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace extensions {
class Extension;
struct InstallWarning;

namespace api {
namespace declarative_net_request {
struct Rule;
}  // namespace declarative_net_request
}  // namespace api

namespace declarative_net_request {
class ParseInfo;
class RulesetMatcher;

struct IndexAndPersistJSONRulesetResult {
 public:
  enum class Status {
    // The ruleset was successfully indexed.
    kSuccess,
    // The ruleset was ignored during indexing since it exceeded the maximum
    // rules limit.
    kIgnore,
    // The ruleset was unsuccessfully indexed and an error was raised.
    kError,
  };

  static IndexAndPersistJSONRulesetResult CreateSuccessResult(
      int ruleset_checksum,
      std::vector<InstallWarning> warnings,
      size_t rules_count,
      size_t regex_rules_count,
      base::TimeDelta index_and_persist_time);
  static IndexAndPersistJSONRulesetResult CreateIgnoreResult(
      std::vector<InstallWarning> warnings);
  static IndexAndPersistJSONRulesetResult CreateErrorResult(std::string error);

  IndexAndPersistJSONRulesetResult(const IndexAndPersistJSONRulesetResult&) =
      delete;
  IndexAndPersistJSONRulesetResult& operator=(
      const IndexAndPersistJSONRulesetResult&) = delete;

  ~IndexAndPersistJSONRulesetResult();

  IndexAndPersistJSONRulesetResult(IndexAndPersistJSONRulesetResult&&);
  IndexAndPersistJSONRulesetResult& operator=(
      IndexAndPersistJSONRulesetResult&&);

  // The result of IndexAndPersistRules.
  Status status = Status::kError;

  // Checksum of the persisted indexed ruleset file. Valid if |status| if
  // kSuccess. Note: there's no sane default value for this, any integer value
  // is a valid checksum value.
  int ruleset_checksum = 0;

  // Valid if |status| is kSuccess or kIgnore.
  std::vector<InstallWarning> warnings;

  // The number of indexed rules. Valid if |status| is kSuccess.
  size_t rules_count = 0;

  // The number of indexed regex rules. Valid if |status| is kSuccess.
  size_t regex_rules_count = 0;

  // Time taken to deserialize the JSON rules and persist them in flatbuffer
  // format. Valid if status is kSuccess.
  base::TimeDelta index_and_persist_time;

  // Valid if |status| is kError.
  std::string error;

 private:
  IndexAndPersistJSONRulesetResult();
};

struct ReadJSONRulesResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kSuccess = 0,
    kFileDoesNotExist = 1,
    kFileReadError = 2,
    kJSONParseError = 3,
    kJSONIsNotList = 4,

    // Status returned when the list of rules to be read exceeds the static rule
    // count limit.
    kRuleCountLimitExceeded = 5,

    // Magic constant used by histograms code. Should be equal to the maximum
    // enum value.
    kMaxValue = kRuleCountLimitExceeded
  };

  static ReadJSONRulesResult CreateErrorResult(Status status,
                                               std::string error);

  ReadJSONRulesResult();
  ReadJSONRulesResult(const ReadJSONRulesResult&) = delete;
  ReadJSONRulesResult(ReadJSONRulesResult&&);

  ReadJSONRulesResult& operator=(const ReadJSONRulesResult&) = delete;
  ReadJSONRulesResult& operator=(ReadJSONRulesResult&&);

  ~ReadJSONRulesResult();

  Status status = Status::kSuccess;

  // Empty in case of an error.
  std::vector<api::declarative_net_request::Rule> rules;

  // Warnings while parsing rules.
  std::vector<InstallWarning> rule_parse_warnings;

  // Populated on error.
  std::string error;
};

// A Ruleset source which is backed on disk. The indexed version of such a
// ruleset undergoes checksum verification on each load and also includes a
// header to recognise the indexed schema version.
class FileBackedRulesetSource : public RulesetSource {
 public:
  enum class RulesetFilter {
    // Only include static rulesets which are enabled by default in the
    // manifest.
    kIncludeManifestEnabled,
    // Include all static rulesets.
    kIncludeAll
  };

  // Creates FileBackedRulesetSources corresponding to the static rulesets in
  // the extension package.
  static std::vector<FileBackedRulesetSource> CreateStatic(
      const Extension& extension,
      RulesetFilter ruleset_filter);

  // Creates a static FileBackedRulesetSource corresponding to |info| for the
  // given |extension|.
  static FileBackedRulesetSource CreateStatic(
      const Extension& extension,
      const DNRManifestData::RulesetInfo& info);

  // Creates FileBackedRulesetSource corresponding to the dynamic rules added by
  // the extension. This must only be called for extensions which specified a
  // declarative ruleset.
  static FileBackedRulesetSource CreateDynamic(content::BrowserContext* context,
                                               const ExtensionId& extension_id);

  // Creates a temporary source i.e. a source corresponding to temporary files.
  // Returns null on failure.
  static std::unique_ptr<FileBackedRulesetSource> CreateTemporarySource(
      RulesetID id,
      size_t rule_count_limit,
      ExtensionId extension_id);

  ~FileBackedRulesetSource() override;
  FileBackedRulesetSource(FileBackedRulesetSource&&);
  FileBackedRulesetSource& operator=(FileBackedRulesetSource&&);

  FileBackedRulesetSource Clone() const;

  // Path to the JSON rules.
  const base::FilePath& json_path() const { return json_path_; }

  // Path to the indexed flatbuffer rules.
  const base::FilePath& indexed_path() const { return indexed_path_; }

  bool is_dynamic_ruleset() const { return id() == kDynamicRulesetID; }

  // Indexes and persists the JSON ruleset. This is potentially unsafe since the
  // JSON rules file is parsed in-process. Note: This must be called on a
  // sequence where file IO is allowed.
  IndexAndPersistJSONRulesetResult IndexAndPersistJSONRulesetUnsafe(
      uint8_t parse_flags) const;

  using IndexAndPersistJSONRulesetCallback =
      base::OnceCallback<void(IndexAndPersistJSONRulesetResult)>;
  // Same as IndexAndPersistJSONRulesetUnsafe but parses the JSON rules file
  // out-of-process. |decoder| corresponds to a Data Decoder service instance
  // to use for decode operations related to this call.
  //
  // NOTE: This must be called on a sequence where file IO is allowed.
  void IndexAndPersistJSONRuleset(
      data_decoder::DataDecoder* decoder,
      uint8_t parse_flags,
      IndexAndPersistJSONRulesetCallback callback) const;

  // Reads JSON rules synchronously. Callers should only use this if the JSON is
  // trusted. Must be called on a sequence which supports file IO.
  ReadJSONRulesResult ReadJSONRulesUnsafe() const;

  // Serializes |rules| into the `json` string. Returns false on failure.
  bool SerializeRulesToJSON(
      const std::vector<api::declarative_net_request::Rule>& rules,
      std::string* json) const;

  // Creates a verified RulesetMatcher corresponding to indexed ruleset on disk.
  // Returns kSuccess on success along with the ruleset |matcher|. Must be
  // called on a sequence which supports file IO.
  LoadRulesetResult CreateVerifiedMatcher(
      int expected_ruleset_checksum,
      std::unique_ptr<RulesetMatcher>* matcher) const;

 private:
  FileBackedRulesetSource(base::FilePath json_path,
                          base::FilePath indexed_path,
                          RulesetID id,
                          size_t rule_count_limit,
                          ExtensionId extension_id,
                          bool enabled);

  base::FilePath json_path_;
  base::FilePath indexed_path_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILE_BACKED_RULESET_SOURCE_H_
