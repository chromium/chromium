// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"

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

struct IndexAndPersistJSONRulesetResult {
 public:
  static IndexAndPersistJSONRulesetResult CreateSuccessResult(
      int ruleset_checksum,
      std::vector<InstallWarning> warnings,
      size_t rules_count,
      base::TimeDelta index_and_persist_time);
  static IndexAndPersistJSONRulesetResult CreateErrorResult(std::string error);

  ~IndexAndPersistJSONRulesetResult();
  IndexAndPersistJSONRulesetResult(IndexAndPersistJSONRulesetResult&&);
  IndexAndPersistJSONRulesetResult& operator=(
      IndexAndPersistJSONRulesetResult&&);

  // Whether IndexAndPersistRules succeeded.
  bool success;

  // Checksum of the persisted indexed ruleset file. Valid if |success| if true.
  int ruleset_checksum;

  // Valid if |success| is true.
  std::vector<InstallWarning> warnings;

  // The number of indexed rules. Valid if |success| is true.
  size_t rules_count;

  // Time taken to deserialize the JSON rules and persist them in flatbuffer
  // format. Valid if success is true.
  base::TimeDelta index_and_persist_time;

  // Valid if |success| is false.
  std::string error;

 private:
  IndexAndPersistJSONRulesetResult();
  DISALLOW_COPY_AND_ASSIGN(IndexAndPersistJSONRulesetResult);
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

    // Magic constant used by histograms code. Should be equal to the maximum
    // enum value.
    kMaxValue = kJSONIsNotList
  };

  static ReadJSONRulesResult CreateErrorResult(Status status,
                                               std::string error);

  ReadJSONRulesResult();
  ~ReadJSONRulesResult();
  ReadJSONRulesResult(ReadJSONRulesResult&&);
  ReadJSONRulesResult& operator=(ReadJSONRulesResult&&);

  Status status = Status::kSuccess;

  // Empty in case of an error.
  std::vector<api::declarative_net_request::Rule> rules;

  // Warnings while parsing rules.
  std::vector<InstallWarning> rule_parse_warnings;

  // Populated on error.
  std::string error;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadJSONRulesResult);
};

// Holds paths for an extension ruleset.
class RulesetSource {
 public:
  const static size_t kStaticRulesetID;
  const static size_t kDynamicRulesetID;

  // Creates RulesetSource corresponding to the static ruleset in the extension
  // package. This must only be called for extensions which specified a
  // declarative ruleset.
  static RulesetSource CreateStatic(const Extension& extension);

  // Creates RulesetSource corresponding to the dynamic rules added by the
  // extension. This must only be called for extensions which specified a
  // declarative ruleset.
  static RulesetSource CreateDynamic(content::BrowserContext* context,
                                     const Extension& extension);

  // Creates a temporary source i.e. a source corresponding to temporary files.
  // Returns null on failure.
  static std::unique_ptr<RulesetSource> CreateTemporarySource(
      size_t id,
      size_t priority,
      api::declarative_net_request::SourceType type,
      size_t rule_count_limit,
      ExtensionId extension_id);

  RulesetSource(base::FilePath json_path,
                base::FilePath indexed_path,
                size_t id,
                size_t priority,
                api::declarative_net_request::SourceType type,
                size_t rule_count_limit,
                ExtensionId extension_id);
  ~RulesetSource();
  RulesetSource(RulesetSource&&);
  RulesetSource& operator=(RulesetSource&&);

  RulesetSource Clone() const;

  // Path to the JSON rules.
  const base::FilePath& json_path() const { return json_path_; }

  // Path to the indexed flatbuffer rules.
  const base::FilePath& indexed_path() const { return indexed_path_; }

  // Each ruleset source within an extension has a distinct ID and priority.
  size_t id() const { return id_; }
  size_t priority() const { return priority_; }

  // The origin type for this ruleset. Can be from the manifest or dynamic.
  api::declarative_net_request::SourceType type() const { return type_; }

  // The maximum number of rules that will be indexed from this source.
  size_t rule_count_limit() const { return rule_count_limit_; }

  // The ID of the extension from which the ruleset originates from.
  const ExtensionId& extension_id() const { return extension_id_; }

  // Indexes and persists the JSON ruleset. This is potentially unsafe since the
  // JSON rules file is parsed in-process. Note: This must be called on a
  // sequence where file IO is allowed.
  IndexAndPersistJSONRulesetResult IndexAndPersistJSONRulesetUnsafe() const;

  using IndexAndPersistJSONRulesetCallback =
      base::OnceCallback<void(IndexAndPersistJSONRulesetResult)>;
  // Same as IndexAndPersistJSONRulesetUnsafe but parses the JSON rules file
  // out-of-process. |decoder| corresponds to a Data Decoder service instance
  // to use for decode operations related to this call.
  //
  // NOTE: This must be called on a sequence where file IO is allowed.
  void IndexAndPersistJSONRuleset(
      data_decoder::DataDecoder* decoder,
      IndexAndPersistJSONRulesetCallback callback) const;

  // Indexes the given |rules| in indexed/flatbuffer format. Populates
  // |ruleset_checksum| on success. The number of |rules| must be less than the
  // rule count limit.
  ParseInfo IndexAndPersistRules(
      std::vector<api::declarative_net_request::Rule> rules,
      int* ruleset_checksum) const;

  // Reads JSON rules synchronously. Callers should only use this if the JSON is
  // trusted. Must be called on a sequence which supports file IO.
  ReadJSONRulesResult ReadJSONRulesUnsafe() const;

  // Serializes and writes |rules| to the json path. Returns false on failure.
  bool WriteRulesToJSON(
      const std::vector<api::declarative_net_request::Rule>& rules) const;

 private:
  base::FilePath json_path_;
  base::FilePath indexed_path_;
  size_t id_;
  size_t priority_;
  api::declarative_net_request::SourceType type_;
  size_t rule_count_limit_;
  ExtensionId extension_id_;

  DISALLOW_COPY_AND_ASSIGN(RulesetSource);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
