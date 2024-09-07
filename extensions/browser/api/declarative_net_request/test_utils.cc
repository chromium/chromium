// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/test_utils.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/not_fatal_until.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::declarative_net_request {

namespace dnr_api = api::declarative_net_request;

RequestAction CreateRequestActionForTesting(RequestAction::Type type,
                                            uint32_t rule_id,
                                            uint32_t rule_priority,
                                            RulesetID ruleset_id,
                                            const ExtensionId& extension_id) {
  dnr_api::RuleActionType action = [type] {
    switch (type) {
      case RequestAction::Type::BLOCK:
      case RequestAction::Type::COLLAPSE:
        return dnr_api::RuleActionType::kBlock;
      case RequestAction::Type::ALLOW:
        return dnr_api::RuleActionType::kAllow;
      case RequestAction::Type::REDIRECT:
        return dnr_api::RuleActionType::kRedirect;
      case RequestAction::Type::UPGRADE:
        return dnr_api::RuleActionType::kUpgradeScheme;
      case RequestAction::Type::ALLOW_ALL_REQUESTS:
        return dnr_api::RuleActionType::kAllowAllRequests;
      case RequestAction::Type::MODIFY_HEADERS:
        return dnr_api::RuleActionType::kModifyHeaders;
    }
  }();
  return RequestAction(type, rule_id,
                       ComputeIndexedRulePriority(rule_priority, action),
                       ruleset_id, extension_id);
}

bool operator==(const RequestAction::HeaderInfo& lhs,
                const RequestAction::HeaderInfo& rhs) {
  return lhs.header == rhs.header && lhs.operation == rhs.operation;
}

std::ostream& operator<<(std::ostream& output,
                         const RequestAction::HeaderInfo& header_info) {
  output << "\nRequestAction::HeaderInfo\n";
  output << "\t|operation| " << dnr_api::ToString(header_info.operation)
         << "\n";
  output << "\t|header| " << header_info.header << "\n";
  output << "\t|value| "
         << (header_info.value ? *header_info.value : std::string("nullopt"));
  return output;
}

// Note: This is not declared in the anonymous namespace so that we can use it
// with gtest. This reuses the logic used to test action equality in
// TestRequestAction in test_utils.h.
bool operator==(const RequestAction& lhs, const RequestAction& rhs) {
  static_assert(flat::IndexType_count == 3,
                "Modify this method to ensure it stays updated as new actions "
                "are added.");

  auto get_members_tuple = [](const RequestAction& action) {
    return std::tie(action.type, action.redirect_url, action.rule_id,
                    action.index_priority, action.ruleset_id,
                    action.extension_id);
  };

  auto are_headers_equal = [](std::vector<RequestAction::HeaderInfo> a,
                              std::vector<RequestAction::HeaderInfo> b) {
    auto header_info_comparator = [](const RequestAction::HeaderInfo& lhs,
                                     const RequestAction::HeaderInfo& rhs) {
      return std::make_tuple(lhs.header, lhs.operation, lhs.value) >
             std::make_tuple(rhs.header, rhs.operation, rhs.value);
    };

    std::sort(a.begin(), a.end(), header_info_comparator);
    std::sort(b.begin(), b.end(), header_info_comparator);

    return a == b;
  };

  return get_members_tuple(lhs) == get_members_tuple(rhs) &&
         are_headers_equal(lhs.request_headers_to_modify,
                           rhs.request_headers_to_modify) &&
         are_headers_equal(lhs.response_headers_to_modify,
                           rhs.response_headers_to_modify);
}

std::ostream& operator<<(std::ostream& output, RequestAction::Type type) {
  switch (type) {
    case RequestAction::Type::BLOCK:
      output << "BLOCK";
      break;
    case RequestAction::Type::COLLAPSE:
      output << "COLLAPSE";
      break;
    case RequestAction::Type::ALLOW:
      output << "ALLOW";
      break;
    case RequestAction::Type::REDIRECT:
      output << "REDIRECT";
      break;
    case RequestAction::Type::UPGRADE:
      output << "UPGRADE";
      break;
    case RequestAction::Type::ALLOW_ALL_REQUESTS:
      output << "ALLOW_ALL_REQUESTS";
      break;
    case RequestAction::Type::MODIFY_HEADERS:
      output << "MODIFY_HEADERS";
      break;
  }
  return output;
}

std::ostream& operator<<(std::ostream& output, const RequestAction& action) {
  output << "\nRequestAction\n";
  output << "|type| " << action.type << "\n";
  output << "|redirect_url| "
         << (action.redirect_url ? action.redirect_url->spec()
                                 : std::string("nullopt"))
         << "\n";
  output << "|rule_id| " << action.rule_id << "\n";
  output << "|index_priority| " << action.index_priority << "\n";
  output << "|ruleset_id| " << action.ruleset_id << "\n";
  output << "|extension_id| " << action.extension_id << "\n";
  output << "|request_headers_to_modify|"
         << ::testing::PrintToString(action.request_headers_to_modify) << "\n";
  output << "|response_headers_to_modify|"
         << ::testing::PrintToString(action.response_headers_to_modify);
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const std::optional<RequestAction>& action) {
  if (!action) {
    return output << "empty Optional<RequestAction>";
  }
  return output << *action;
}

std::ostream& operator<<(std::ostream& output, const ParseResult& result) {
  switch (result) {
    case ParseResult::NONE:
      output << "NONE";
      break;
    case ParseResult::SUCCESS:
      output << "SUCCESS";
      break;
    case ParseResult::ERROR_REQUEST_METHOD_DUPLICATED:
      output << "ERROR_REQUEST_METHOD_DUPLICATED";
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      output << "ERROR_RESOURCE_TYPE_DUPLICATED";
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      output << "ERROR_INVALID_RULE_ID";
      break;
    case ParseResult::ERROR_INVALID_RULE_PRIORITY:
      output << "ERROR_INVALID_RULE_PRIORITY";
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      output << "ERROR_NO_APPLICABLE_RESOURCE_TYPES";
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      output << "ERROR_EMPTY_DOMAINS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_INITIATOR_DOMAINS_LIST:
      output << "ERROR_EMPTY_INITIATOR_DOMAINS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_REQUEST_DOMAINS_LIST:
      output << "ERROR_EMPTY_REQUEST_DOMAINS_LIST";
      break;
    case ParseResult::ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED:
      output << "ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED";
      break;
    case ParseResult::
        ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED:
      output << "ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_"
                "SPECIFIED";
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      output << "ERROR_EMPTY_RESOURCE_TYPES_LIST";
      break;
    case ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST:
      output << "ERROR_EMPTY_REQUEST_METHODS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      output << "ERROR_EMPTY_URL_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      output << "ERROR_INVALID_REDIRECT_URL";
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      output << "ERROR_DUPLICATE_IDS";
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      output << "ERROR_NON_ASCII_URL_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      output << "ERROR_NON_ASCII_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      output << "ERROR_NON_ASCII_EXCLUDED_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_INITIATOR_DOMAIN:
      output << "ERROR_NON_ASCII_INITIATOR_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN:
      output << "ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_REQUEST_DOMAIN:
      output << "ERROR_NON_ASCII_REQUEST_DOMAIN";
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN:
      output << "ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN";
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      output << "ERROR_INVALID_URL_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REDIRECT:
      output << "ERROR_INVALID_REDIRECT";
      break;
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      output << "ERROR_INVALID_EXTENSION_PATH";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      output << "ERROR_INVALID_TRANSFORM_SCHEME";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      output << "ERROR_INVALID_TRANSFORM_PORT";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      output << "ERROR_INVALID_TRANSFORM_QUERY";
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      output << "ERROR_INVALID_TRANSFORM_FRAGMENT";
      break;
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      output << "ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED";
      break;
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      output << "ERROR_JAVASCRIPT_REDIRECT";
      break;
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      output << "ERROR_EMPTY_REGEX_FILTER";
      break;
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      output << "ERROR_NON_ASCII_REGEX_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      output << "ERROR_INVALID_REGEX_FILTER";
      break;
    case ParseResult::ERROR_REGEX_TOO_LARGE:
      output << "ERROR_REGEX_TOO_LARGE";
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      output << "ERROR_MULTIPLE_FILTERS_SPECIFIED";
      break;
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      output << "ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER";
      break;
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      output << "ERROR_INVALID_REGEX_SUBSTITUTION";
      break;
    case ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE:
      output << "ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE";
      break;
    case ParseResult::ERROR_NO_HEADERS_TO_MODIFY_SPECIFIED:
      output << "ERROR_NO_HEADERS_TO_MODIFY_SPECIFIED";
      break;
    case ParseResult::ERROR_EMPTY_MODIFY_REQUEST_HEADERS_LIST:
      output << "ERROR_EMPTY_MODIFY_REQUEST_HEADERS_LIST";
      break;
    case ParseResult::ERROR_EMPTY_MODIFY_RESPONSE_HEADERS_LIST:
      output << "ERROR_EMPTY_MODIFY_RESPONSE_HEADERS_LIST";
      break;
    case ParseResult::ERROR_INVALID_HEADER_TO_MODIFY_NAME:
      output << "ERROR_INVALID_HEADER_TO_MODIFY_NAME";
      break;
    case ParseResult::ERROR_INVALID_HEADER_TO_MODIFY_VALUE:
      output << "ERROR_INVALID_HEADER_TO_MODIFY_VALUE";
      break;
    case ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED:
      output << "ERROR_HEADER_VALUE_NOT_SPECIFIED";
      break;
    case ParseResult::ERROR_HEADER_VALUE_PRESENT:
      output << "ERROR_HEADER_VALUE_PRESENT";
      break;
    case ParseResult::ERROR_APPEND_INVALID_REQUEST_HEADER:
      output << "ERROR_APPEND_INVALID_REQUEST_HEADER";
      break;
    case ParseResult::ERROR_EMPTY_TAB_IDS_LIST:
      output << "ERROR_EMPTY_TAB_IDS_LIST";
      break;
    case ParseResult::ERROR_TAB_IDS_ON_NON_SESSION_RULE:
      output << "ERROR_TAB_IDS_ON_NON_SESSION_RULE";
      break;
    case ParseResult::ERROR_TAB_ID_DUPLICATED:
      output << "ERROR_TAB_ID_DUPLICATED";
      break;
    case ParseResult::ERROR_EMPTY_RESPONSE_HEADER_MATCHING_LIST:
      output << "ERROR_EMPTY_RESPONSE_HEADER_MATCHING_LIST";
      break;
    case ParseResult::ERROR_EMPTY_EXCLUDED_RESPONSE_HEADER_MATCHING_LIST:
      output << "ERROR_EMPTY_EXCLUDED_RESPONSE_HEADER_MATCHING_LIST";
      break;
    case ParseResult::ERROR_INVALID_MATCHING_RESPONSE_HEADER_NAME:
      output << "ERROR_INVALID_MATCHING_RESPONSE_HEADER_NAME";
      break;
    case ParseResult::ERROR_INVALID_MATCHING_EXCLUDED_RESPONSE_HEADER_NAME:
      output << "ERROR_INVALID_MATCHING_EXCLUDED_RESPONSE_HEADER_NAME";
      break;
    case ParseResult::ERROR_INVALID_MATCHING_RESPONSE_HEADER_VALUE:
      output << "ERROR_INVALID_MATCHING_RESPONSE_HEADER_VALUE";
      break;
    case ParseResult::ERROR_MATCHING_RESPONSE_HEADER_DUPLICATED:
      output << "ERROR_MATCHING_RESPONSE_HEADER_DUPLICATED";
      break;
    case ParseResult::ERROR_RESPONSE_HEADER_RULE_CANNOT_MODIFY_REQUEST_HEADERS:
      output << "ERROR_RESPONSE_HEADER_RULE_CANNOT_MODIFY_REQUEST_HEADERS";
      break;
  }
  return output;
}

std::ostream& operator<<(std::ostream& output, LoadRulesetResult result) {
  switch (result) {
    case LoadRulesetResult::kSuccess:
      output << "kSuccess";
      break;
    case LoadRulesetResult::kErrorInvalidPath:
      output << "kErrorInvalidPath";
      break;
    case LoadRulesetResult::kErrorCannotReadFile:
      output << "kErrorCannotReadFile";
      break;
    case LoadRulesetResult::kErrorChecksumMismatch:
      output << "kErrorChecksumMismatch";
      break;
    case LoadRulesetResult::kErrorVersionMismatch:
      output << "kErrorVersionMismatch";
      break;
    case LoadRulesetResult::kErrorChecksumNotFound:
      output << "kErrorChecksumNotFound";
      break;
  }
  return output;
}

std::ostream& operator<<(std::ostream& output, const RuleCounts& count) {
  output << "\nRuleCounts\n";
  output << "|rule_count| " << count.rule_count << "\n";
  if (count.unsafe_rule_count.has_value()) {
    output << "|unsafe_rule_count| " << *(count.unsafe_rule_count) << "\n";
  }
  output << "|regex_rule_count| " << count.regex_rule_count << "\n";
  return output;
}

bool AreAllIndexedStaticRulesetsValid(
    const Extension& extension,
    content::BrowserContext* browser_context,
    FileBackedRulesetSource::RulesetFilter ruleset_filter) {
  std::vector<FileBackedRulesetSource> sources =
      FileBackedRulesetSource::CreateStatic(extension, ruleset_filter);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  PrefsHelper helper(*prefs);

  for (const auto& source : sources) {
    if (helper.ShouldIgnoreRuleset(extension.id(), source.id())) {
      continue;
    }

    int expected_checksum = -1;
    if (!helper.GetStaticRulesetChecksum(extension.id(), source.id(),
                                         expected_checksum)) {
      return false;
    }

    std::unique_ptr<RulesetMatcher> matcher;
    if (source.CreateVerifiedMatcher(expected_checksum, &matcher) !=
        LoadRulesetResult::kSuccess) {
      return false;
    }
  }

  return true;
}

bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const FileBackedRulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum) {
  using IndexStatus = IndexAndPersistJSONRulesetResult::Status;

  // Serialize |rules|.
  base::Value::List builder;
  for (const auto& rule : rules) {
    builder.Append(rule.ToValue());
  }
  JSONFileValueSerializer(source.json_path()).Serialize(std::move(builder));

  // Index ruleset.
  auto parse_flags = FileBackedRulesetSource::kRaiseErrorOnInvalidRules |
                     FileBackedRulesetSource::kRaiseWarningOnLargeRegexRules;
  IndexAndPersistJSONRulesetResult result =
      source.IndexAndPersistJSONRulesetUnsafe(parse_flags);
  if (result.status == IndexStatus::kError) {
    DCHECK(result.error.empty()) << result.error;
    return false;
  }

  if (!result.warnings.empty()) {
    return false;
  }

  DCHECK_EQ(IndexStatus::kSuccess, result.status);
  if (expected_checksum) {
    *expected_checksum = result.ruleset_checksum;
  }

  LoadRulesetResult load_result =
      source.CreateVerifiedMatcher(result.ruleset_checksum, matcher);
  return load_result == LoadRulesetResult::kSuccess;
}

FileBackedRulesetSource CreateTemporarySource(RulesetID id,
                                              size_t rule_count_limit,
                                              ExtensionId extension_id) {
  std::unique_ptr<FileBackedRulesetSource> source =
      FileBackedRulesetSource::CreateTemporarySource(id, rule_count_limit,
                                                     std::move(extension_id));
  CHECK(source);
  return source->Clone();
}

dnr_api::ModifyHeaderInfo CreateModifyHeaderInfo(
    dnr_api::HeaderOperation operation,
    std::string header,
    std::optional<std::string> value,
    std::optional<std::string> regex_filter,
    std::optional<std::string> regex_substitution,
    std::optional<dnr_api::HeaderRegexOptions> regex_options) {
  dnr_api::ModifyHeaderInfo header_info;

  header_info.operation = std::move(operation);
  header_info.header = std::move(header);
  header_info.value = std::move(value);
  header_info.regex_filter = std::move(regex_filter);
  header_info.regex_substitution = std::move(regex_substitution);
  header_info.regex_options = std::move(regex_options);

  return header_info;
}

bool EqualsForTesting(const dnr_api::ModifyHeaderInfo& lhs,
                      const dnr_api::ModifyHeaderInfo& rhs) {
  bool are_values_equal = lhs.value && rhs.value ? *lhs.value == *rhs.value
                                                 : lhs.value == rhs.value;
  return lhs.operation == rhs.operation && lhs.header == rhs.header &&
         are_values_equal;
}

dnr_api::HeaderInfo CreateHeaderInfo(
    std::string header,
    std::optional<std::vector<std::string>> values,
    std::optional<std::vector<std::string>> excluded_values) {
  dnr_api::HeaderInfo header_info;

  header_info.header = std::move(header);
  header_info.values = std::move(values);
  header_info.excluded_values = std::move(excluded_values);

  return header_info;
}

RulesetManagerObserver::RulesetManagerObserver(RulesetManager* manager)
    : manager_(manager), current_count_(manager_->GetMatcherCountForTest()) {
  manager_->SetObserverForTest(this);
}

RulesetManagerObserver::~RulesetManagerObserver() {
  manager_->SetObserverForTest(nullptr);
}

std::vector<GURL> RulesetManagerObserver::GetAndResetRequestSeen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<GURL> seen_requests;
  std::swap(seen_requests, observed_requests_);
  return seen_requests;
}

void RulesetManagerObserver::WaitForExtensionsWithRulesetsCount(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ASSERT_FALSE(expected_count_);
  if (current_count_ == count) {
    return;
  }

  expected_count_ = count;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
}

void RulesetManagerObserver::OnRulesetCountChanged(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_count_ = count;
  if (expected_count_ != count) {
    return;
  }

  ASSERT_TRUE(run_loop_.get());

  run_loop_->Quit();
  expected_count_.reset();
}

void RulesetManagerObserver::OnEvaluateRequest(const WebRequestInfo& request,
                                               bool is_incognito_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observed_requests_.push_back(request.url);
}

WarningServiceObserver::WarningServiceObserver(WarningService* warning_service,
                                               const ExtensionId& extension_id)
    : extension_id_(extension_id) {
  observation_.Observe(warning_service);
}

WarningServiceObserver::~WarningServiceObserver() = default;

void WarningServiceObserver::WaitForWarning() {
  run_loop_.Run();
}

void WarningServiceObserver::ExtensionWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  if (!base::Contains(affected_extensions, extension_id_)) {
    return;
  }

  run_loop_.Quit();
}

base::flat_set<int> GetDisabledRuleIdsFromMatcherForTesting(
    const RulesetManager& ruleset_manager,
    const Extension& extension,
    const std::string& ruleset_id_string) {
  const DNRManifestData::ManifestIDToRulesetMap& public_id_map =
      DNRManifestData::GetManifestIDToRulesetMap(extension);
  auto it = public_id_map.find(ruleset_id_string);
  CHECK(public_id_map.end() != it, base::NotFatalUntil::M130);
  RulesetID ruleset_id = it->second->id;

  const CompositeMatcher* composite_matcher =
      ruleset_manager.GetMatcherForExtension(extension.id());
  DCHECK(composite_matcher);

  for (const auto& matcher : composite_matcher->matchers()) {
    if (ruleset_id != matcher->id()) {
      continue;
    }

    return matcher->GetDisabledRuleIdsForTesting();
  }
  return {};
}

RequestParams CreateRequestWithResponseHeaders(
    const GURL& url,
    const net::HttpResponseHeaders* headers) {
  return RequestParams(url, url::Origin(), dnr_api::ResourceType::kSubFrame,
                       dnr_api::RequestMethod::kGet, -1, headers);
}

}  // namespace extensions::declarative_net_request
