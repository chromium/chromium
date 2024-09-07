// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_features.h"
#include "net/http/http_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions::declarative_net_request {

namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = extensions::api::declarative_net_request;

constexpr char kAnchorCharacter = '|';
constexpr char kSeparatorCharacter = '^';
constexpr char kWildcardCharacter = '*';
constexpr int kLargeRegexUMALimit = 1024 * 100;

// Returns true if bitmask |sub| is a subset of |super|.
constexpr bool IsSubset(unsigned sub, unsigned super) {
  return (super | sub) == super;
}

// Helper class to parse the url filter of a Declarative Net Request API rule.
class UrlFilterParser {
 public:
  UrlFilterParser(const UrlFilterParser&) = delete;
  UrlFilterParser& operator=(const UrlFilterParser&) = delete;

  // This sets the |url_pattern_type|, |anchor_left|, |anchor_right| and
  // |url_pattern| fields on the |indexed_rule_|.
  static void Parse(std::optional<std::string> url_filter,
                    IndexedRule* indexed_rule) {
    DCHECK(indexed_rule);
    UrlFilterParser(url_filter ? std::move(*url_filter) : std::string(),
                    indexed_rule)
        .ParseImpl();
  }

 private:
  UrlFilterParser(std::string url_filter, IndexedRule* indexed_rule)
      : url_filter_(std::move(url_filter)),
        url_filter_len_(url_filter_.length()),
        index_(0),
        indexed_rule_(indexed_rule) {}

  void ParseImpl() {
    ParseLeftAnchor();
    DCHECK_LE(index_, 2u);

    ParseFilterString();
    DCHECK(index_ == url_filter_len_ || index_ + 1 == url_filter_len_);

    ParseRightAnchor();
    DCHECK_EQ(url_filter_len_, index_);
  }

  void ParseLeftAnchor() {
    indexed_rule_->anchor_left = flat_rule::AnchorType_NONE;

    if (IsAtAnchor()) {
      ++index_;
      indexed_rule_->anchor_left = flat_rule::AnchorType_BOUNDARY;
      if (IsAtAnchor()) {
        ++index_;
        indexed_rule_->anchor_left = flat_rule::AnchorType_SUBDOMAIN;
      }
    }
  }

  void ParseFilterString() {
    indexed_rule_->url_pattern_type = flat_rule::UrlPatternType_SUBSTRING;
    size_t left_index = index_;
    while (index_ < url_filter_len_ && !IsAtRightAnchor()) {
      if (IsAtSeparatorOrWildcard()) {
        indexed_rule_->url_pattern_type = flat_rule::UrlPatternType_WILDCARDED;
      }
      ++index_;
    }
    // Note: Empty url patterns are supported.
    indexed_rule_->url_pattern =
        url_filter_.substr(left_index, index_ - left_index);
  }

  void ParseRightAnchor() {
    indexed_rule_->anchor_right = flat_rule::AnchorType_NONE;
    if (IsAtRightAnchor()) {
      ++index_;
      indexed_rule_->anchor_right = flat_rule::AnchorType_BOUNDARY;
    }
  }

  bool IsAtSeparatorOrWildcard() const {
    return IsAtValidIndex() && (url_filter_[index_] == kSeparatorCharacter ||
                                url_filter_[index_] == kWildcardCharacter);
  }

  bool IsAtRightAnchor() const {
    return IsAtAnchor() && index_ > 0 && index_ + 1 == url_filter_len_;
  }

  bool IsAtValidIndex() const { return index_ < url_filter_len_; }

  bool IsAtAnchor() const {
    return IsAtValidIndex() && url_filter_[index_] == kAnchorCharacter;
  }

  const std::string url_filter_;
  const size_t url_filter_len_;
  size_t index_;
  raw_ptr<IndexedRule> indexed_rule_;  // Must outlive this instance.
};

bool IsCaseSensitive(const dnr_api::Rule& parsed_rule) {
  // If case sensitivity is not explicitly specified, rules are considered case
  // insensitive by default.
  if (!parsed_rule.condition.is_url_filter_case_sensitive) {
    return false;
  }

  return *parsed_rule.condition.is_url_filter_case_sensitive;
}

// Returns a bitmask of flat_rule::OptionFlag corresponding to |parsed_rule|.
uint8_t GetOptionsMask(const dnr_api::Rule& parsed_rule) {
  uint8_t mask = flat_rule::OptionFlag_NONE;

  if (parsed_rule.action.type == dnr_api::RuleActionType::kAllow) {
    mask |= flat_rule::OptionFlag_IS_ALLOWLIST;
  }

  if (!IsCaseSensitive(parsed_rule)) {
    mask |= flat_rule::OptionFlag_IS_CASE_INSENSITIVE;
  }

  switch (parsed_rule.condition.domain_type) {
    case dnr_api::DomainType::kFirstParty:
      mask |= flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY;
      break;
    case dnr_api::DomainType::kThirdParty:
      mask |= flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY;
      break;
    case dnr_api::DomainType::kNone:
      mask |= (flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY |
               flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY);
      break;
  }
  return mask;
}

uint8_t GetActivationTypes(const dnr_api::Rule& parsed_rule) {
  // Extensions don't use any activation types currently.
  return flat_rule::ActivationType_NONE;
}

// Returns a bitmask of flat_rule::RequestMethod corresponding to passed
// `request_methods`.
uint16_t GetRequestMethodsMask(
    const std::optional<std::vector<dnr_api::RequestMethod>>& request_methods) {
  uint16_t mask = flat_rule::RequestMethod_NONE;
  if (!request_methods) {
    return mask;
  }

  for (const auto request_method : *request_methods) {
    mask |= GetRequestMethod(request_method);
  }
  return mask;
}

// Computes the bitmask of flat_rule::RequestMethod taking into consideration
// the included and excluded request methods for `rule`.
ParseResult ComputeRequestMethods(const dnr_api::Rule& rule,
                                  uint16_t* request_methods_mask) {
  uint16_t include_request_method_mask =
      GetRequestMethodsMask(rule.condition.request_methods);
  uint16_t exclude_request_method_mask =
      GetRequestMethodsMask(rule.condition.excluded_request_methods);

  if (include_request_method_mask & exclude_request_method_mask) {
    return ParseResult::ERROR_REQUEST_METHOD_DUPLICATED;
  }

  if (include_request_method_mask != flat_rule::RequestMethod_NONE) {
    *request_methods_mask = include_request_method_mask;
  } else if (exclude_request_method_mask != flat_rule::RequestMethod_NONE) {
    *request_methods_mask =
        flat_rule::RequestMethod_ANY & ~exclude_request_method_mask;
  } else {
    *request_methods_mask = flat_rule::RequestMethod_ANY;
  }

  return ParseResult::SUCCESS;
}

// Returns a bitmask of flat_rule::ElementType corresponding to passed
// |resource_types|.
uint16_t GetResourceTypesMask(
    const std::optional<std::vector<dnr_api::ResourceType>>& resource_types) {
  uint16_t mask = flat_rule::ElementType_NONE;
  if (!resource_types) {
    return mask;
  }

  for (const auto resource_type : *resource_types) {
    mask |= GetElementType(resource_type);
  }
  return mask;
}

// Computes the bitmask of flat_rule::ElementType taking into consideration the
// included and excluded resource types for |rule| and its associated action
// type.
ParseResult ComputeElementTypes(const dnr_api::Rule& rule,
                                uint16_t* element_types) {
  uint16_t include_element_type_mask =
      GetResourceTypesMask(rule.condition.resource_types);
  uint16_t exclude_element_type_mask =
      GetResourceTypesMask(rule.condition.excluded_resource_types);

  // OBJECT_SUBREQUEST is not used by Extensions.
  if (exclude_element_type_mask ==
      (flat_rule::ElementType_ANY &
       ~flat_rule::ElementType_OBJECT_SUBREQUEST)) {
    return ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES;
  }

  if (include_element_type_mask & exclude_element_type_mask) {
    return ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED;
  }

  if (rule.action.type == dnr_api::RuleActionType::kAllowAllRequests) {
    // For allowAllRequests rule, the resourceTypes key must always be specified
    // and may only include main_frame and sub_frame types.
    const uint16_t frame_element_type_mask =
        flat_rule::ElementType_MAIN_FRAME | flat_rule::ElementType_SUBDOCUMENT;
    if (include_element_type_mask == flat_rule::ElementType_NONE ||
        !IsSubset(include_element_type_mask, frame_element_type_mask)) {
      return ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE;
    }
  }

  if (include_element_type_mask != flat_rule::ElementType_NONE) {
    *element_types = include_element_type_mask;
  } else if (exclude_element_type_mask != flat_rule::ElementType_NONE) {
    *element_types = flat_rule::ElementType_ANY & ~exclude_element_type_mask;
  } else {
    *element_types = url_pattern_index::kDefaultFlatElementTypesMask;
  }

  return ParseResult::SUCCESS;
}

// Lower-cases and sorts |domains|, as required by the url_pattern_index
// component and stores the result in |output|. Returns false in case of
// failure, when one of the input strings contains non-ascii characters.
bool CanonicalizeDomains(std::optional<std::vector<std::string>> domains,
                         std::vector<std::string>* output) {
  DCHECK(output);
  DCHECK(output->empty());

  if (!domains) {
    return true;
  }

  // Convert to lower case as required by the url_pattern_index component.
  for (const std::string& domain : *domains) {
    if (!base::IsStringASCII(domain)) {
      return false;
    }

    output->push_back(base::ToLowerASCII(domain));
  }

  std::sort(output->begin(), output->end(),
            [](const std::string& left, const std::string& right) {
              return url_pattern_index::CompareDomains(left, right) < 0;
            });

  return true;
}

// Returns if the redirect URL will be used as a relative URL.
bool IsRedirectUrlRelative(const std::string& redirect_url) {
  return !redirect_url.empty() && redirect_url[0] == '/';
}

bool IsValidTransformScheme(const std::optional<std::string>& scheme) {
  if (!scheme) {
    return true;
  }

  for (auto* kAllowedTransformScheme : kAllowedTransformSchemes) {
    if (*scheme == kAllowedTransformScheme) {
      return true;
    }
  }
  return false;
}

bool IsValidPort(const std::optional<std::string>& port) {
  if (!port || port->empty()) {
    return true;
  }

  unsigned port_num = 0;
  return base::StringToUint(*port, &port_num) && port_num <= 65535;
}

bool IsEmptyOrStartsWith(const std::optional<std::string>& str,
                         char starts_with) {
  return !str || str->empty() || str->at(0) == starts_with;
}

// Validates the given url |transform|.
ParseResult ValidateTransform(const dnr_api::URLTransform& transform) {
  if (!IsValidTransformScheme(transform.scheme)) {
    return ParseResult::ERROR_INVALID_TRANSFORM_SCHEME;
  }

  if (!IsValidPort(transform.port)) {
    return ParseResult::ERROR_INVALID_TRANSFORM_PORT;
  }

  if (!IsEmptyOrStartsWith(transform.query, '?')) {
    return ParseResult::ERROR_INVALID_TRANSFORM_QUERY;
  }

  if (!IsEmptyOrStartsWith(transform.fragment, '#')) {
    return ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT;
  }

  // Only one of |query| or |query_transform| should be specified.
  if (transform.query && transform.query_transform) {
    return ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED;
  }

  return ParseResult::SUCCESS;
}

// Parses the "action.redirect" dictionary of a dnr_api::Rule.
ParseResult ParseRedirect(dnr_api::Redirect redirect,
                          const GURL& base_url,
                          IndexedRule* indexed_rule) {
  DCHECK(indexed_rule);

  if (redirect.url) {
    GURL redirect_url = GURL(*redirect.url);
    if (!redirect_url.is_valid()) {
      return ParseResult::ERROR_INVALID_REDIRECT_URL;
    }

    if (redirect_url.SchemeIs(url::kJavaScriptScheme)) {
      return ParseResult::ERROR_JAVASCRIPT_REDIRECT;
    }

    indexed_rule->redirect_url = std::move(*redirect.url);
    return ParseResult::SUCCESS;
  }

  if (redirect.extension_path) {
    if (!IsRedirectUrlRelative(*redirect.extension_path)) {
      return ParseResult::ERROR_INVALID_EXTENSION_PATH;
    }

    GURL redirect_url = base_url.Resolve(*redirect.extension_path);

    // Sanity check that Resolve works as expected.
    DCHECK_EQ(base_url.DeprecatedGetOriginAsURL(),
              redirect_url.DeprecatedGetOriginAsURL());

    if (!redirect_url.is_valid()) {
      return ParseResult::ERROR_INVALID_EXTENSION_PATH;
    }

    indexed_rule->redirect_url = redirect_url.spec();
    return ParseResult::SUCCESS;
  }

  if (redirect.transform) {
    indexed_rule->url_transform = std::move(*redirect.transform);
    return ValidateTransform(*indexed_rule->url_transform);
  }

  if (redirect.regex_substitution) {
    if (redirect.regex_substitution->empty()) {
      return ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION;
    }

    indexed_rule->regex_substitution = std::move(*redirect.regex_substitution);
    return ParseResult::SUCCESS;
  }

  return ParseResult::ERROR_INVALID_REDIRECT;
}

uint8_t GetActionTypePriority(dnr_api::RuleActionType action_type) {
  switch (action_type) {
    case dnr_api::RuleActionType::kAllow:
      return 5;
    case dnr_api::RuleActionType::kAllowAllRequests:
      return 4;
    case dnr_api::RuleActionType::kBlock:
      return 3;
    case dnr_api::RuleActionType::kUpgradeScheme:
      return 2;
    case dnr_api::RuleActionType::kRedirect:
      return 1;
    case dnr_api::RuleActionType::kModifyHeaders:
      return 0;
    case dnr_api::RuleActionType::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

void RecordLargeRegexUMA(bool is_large_regex) {
  UMA_HISTOGRAM_BOOLEAN(kIsLargeRegexHistogram, is_large_regex);
}

void RecordRegexRuleSizeUMA(int program_size) {
  // Max reported size at 100KB.
  UMA_HISTOGRAM_COUNTS_100000(kRegexRuleSizeHistogram, program_size);
}

void RecordRuleSizeForLargeRegex(const std::string& regex_string,
                                 bool is_case_sensitive,
                                 bool require_capturing) {
  re2::RE2::Options large_regex_options =
      CreateRE2Options(is_case_sensitive, require_capturing);

  // Record the size of regex rules that exceed the 2Kb limit, with any rules
  // exceeding 100Kb recorded as 100Kb. Note that these rules are not enabled.
  large_regex_options.set_max_mem(kLargeRegexUMALimit);
  re2::RE2 regex(regex_string, large_regex_options);
  if (regex.error_code() == re2::RE2::ErrorPatternTooLarge) {
    RecordRegexRuleSizeUMA(kLargeRegexUMALimit);
  } else if (regex.ok()) {
    RecordRegexRuleSizeUMA(regex.ProgramSize());
  }
}

// Validates the parsed `regex_filter` and `regex_substitution` and returns a
// ParseResult.
ParseResult ValidateRegex(
    bool is_case_sensitive,
    const std::optional<std::string>& regex_filter,
    const std::optional<std::string>& regex_substitution) {
  if (!regex_filter.has_value()) {
    return regex_substitution.has_value()
               ? ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER
               : ParseResult::SUCCESS;
  }

  if (regex_filter->empty()) {
    return ParseResult::ERROR_EMPTY_REGEX_FILTER;
  }

  if (!base::IsStringASCII(*regex_filter)) {
    return ParseResult::ERROR_NON_ASCII_REGEX_FILTER;
  }

  bool require_capturing = regex_substitution.has_value();

  // TODO(karandeepb): Regex compilation can be expensive. Also, these need to
  // be compiled again once the ruleset is loaded, which means duplicate work.
  // We should maintain a global cache of compiled regexes.
  re2::RE2 regex(*regex_filter,
                 CreateRE2Options(is_case_sensitive, require_capturing));

  if (regex.error_code() == re2::RE2::ErrorPatternTooLarge) {
    RecordLargeRegexUMA(true);
    RecordRuleSizeForLargeRegex(*regex_filter, is_case_sensitive,
                                require_capturing);

    return ParseResult::ERROR_REGEX_TOO_LARGE;
  }

  if (!regex.ok()) {
    return ParseResult::ERROR_INVALID_REGEX_FILTER;
  }

  std::string error;
  if (regex_substitution &&
      !regex.CheckRewriteString(*regex_substitution, &error)) {
    return ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION;
  }

  RecordRegexRuleSizeUMA(regex.ProgramSize());
  RecordLargeRegexUMA(false);

  return ParseResult::SUCCESS;
}

ParseResult ValidateHeadersForModification(
    const std::vector<dnr_api::ModifyHeaderInfo>& headers,
    bool are_request_headers) {
  if (headers.empty()) {
    return are_request_headers
               ? ParseResult::ERROR_EMPTY_MODIFY_REQUEST_HEADERS_LIST
               : ParseResult::ERROR_EMPTY_MODIFY_RESPONSE_HEADERS_LIST;
  }

  for (const auto& header_info : headers) {
    if (!net::HttpUtil::IsValidHeaderName(header_info.header)) {
      return ParseResult::ERROR_INVALID_HEADER_TO_MODIFY_NAME;
    }

    if (are_request_headers &&
        header_info.operation == dnr_api::HeaderOperation::kAppend) {
      DCHECK(
          base::ranges::none_of(header_info.header, base::IsAsciiUpper<char>));
      if (!base::Contains(kDNRRequestHeaderAppendAllowList,
                          header_info.header)) {
        return ParseResult::ERROR_APPEND_INVALID_REQUEST_HEADER;
      }
    }

    if (header_info.value) {
      if (!net::HttpUtil::IsValidHeaderValue(*header_info.value)) {
        return ParseResult::ERROR_INVALID_HEADER_TO_MODIFY_VALUE;
      }

      // Check that a remove operation must not specify a value.
      if (header_info.operation == dnr_api::HeaderOperation::kRemove) {
        return ParseResult::ERROR_HEADER_VALUE_PRESENT;
      }
    } else if (header_info.operation == dnr_api::HeaderOperation::kAppend ||
               header_info.operation == dnr_api::HeaderOperation::kSet) {
      // Check that an append or set operation must specify a value.
      return ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED;
    }

    // Validate the regex filter and substitution inside `header_info` if they
    // exist.
    ParseResult validate_regex_result =
        ValidateRegex(/*is_case_sensitive=*/false, header_info.regex_filter,
                      header_info.regex_substitution);

    if (validate_regex_result != ParseResult::SUCCESS) {
      return validate_regex_result;
    }
  }

  return ParseResult::SUCCESS;
}

void ParseTabIds(const std::vector<int>* input_tab_ids,
                 base::flat_set<int>& output_tab_ids) {
  if (!input_tab_ids) {
    return;
  }

  output_tab_ids =
      base::flat_set<int>(input_tab_ids->begin(), input_tab_ids->end());
}

ParseResult ValidateMatchingResponseHeaderValues(
    const dnr_api::HeaderInfo& header_info) {
  auto validate_header_values = [](const std::vector<std::string>& values) {
    return base::ranges::all_of(values, [](const std::string& value) {
      return net::HttpUtil::IsValidHeaderValue(value);
    });
  };

  if (header_info.values && !validate_header_values(*header_info.values)) {
    return ParseResult::ERROR_INVALID_MATCHING_RESPONSE_HEADER_VALUE;
  }

  if (header_info.excluded_values &&
      !validate_header_values(*header_info.excluded_values)) {
    return ParseResult::ERROR_INVALID_MATCHING_RESPONSE_HEADER_VALUE;
  }

  return ParseResult::SUCCESS;
}

ParseResult ValidateResponseHeadersForMatching(
    const std::vector<dnr_api::HeaderInfo>& response_headers,
    const std::vector<dnr_api::HeaderInfo>& excluded_response_headers) {
  // Track the set of response headers to match. This is used to make sure that
  // the header is not matched in `excluded_response_headers` in name only.
  std::set<std::string> response_header_names;

  for (const auto& header_info : response_headers) {
    if (!net::HttpUtil::IsValidHeaderName(header_info.header)) {
      return ParseResult::ERROR_INVALID_MATCHING_RESPONSE_HEADER_NAME;
    }

    ParseResult result = ValidateMatchingResponseHeaderValues(header_info);
    if (result != ParseResult::SUCCESS) {
      return result;
    }

    response_header_names.insert(header_info.header);
  }

  for (const auto& header_info : excluded_response_headers) {
    if (!net::HttpUtil::IsValidHeaderName(header_info.header)) {
      return ParseResult::ERROR_INVALID_MATCHING_EXCLUDED_RESPONSE_HEADER_NAME;
    }

    ParseResult result = ValidateMatchingResponseHeaderValues(header_info);
    if (result != ParseResult::SUCCESS) {
      return result;
    }

    // Return an error if a rule tries to match on the existence AND
    // non-existence of a header.
    if (!header_info.values.has_value() &&
        !header_info.excluded_values.has_value() &&
        base::Contains(response_header_names, header_info.header)) {
      return ParseResult::ERROR_MATCHING_RESPONSE_HEADER_DUPLICATED;
    }
  }

  return ParseResult::SUCCESS;
}

// For each ModifyHeaderInfo in `header_infos`, if its `regex_options` is
// specified, populate it with default values for any unspecified fields.
void PopulateHeaderRegexOptions(
    std::vector<dnr_api::ModifyHeaderInfo>& header_infos) {
  for (auto& header_info : header_infos) {
    if (auto& options = header_info.regex_options) {
      options->match_all = options->match_all.value_or(false);
    }
  }
}

}  // namespace

IndexedRule::IndexedRule() = default;
IndexedRule::~IndexedRule() = default;
IndexedRule::IndexedRule(IndexedRule&& other) = default;
IndexedRule& IndexedRule::operator=(IndexedRule&& other) = default;

// static
ParseResult IndexedRule::CreateIndexedRule(dnr_api::Rule parsed_rule,
                                           const GURL& base_url,
                                           RulesetID ruleset_id,
                                           IndexedRule* indexed_rule) {
  DCHECK(indexed_rule);

  if (parsed_rule.id < kMinValidID) {
    return ParseResult::ERROR_INVALID_RULE_ID;
  }

  int priority =
      parsed_rule.priority ? *parsed_rule.priority : kDefaultPriority;
  if (priority < kMinValidPriority) {
    return ParseResult::ERROR_INVALID_RULE_PRIORITY;
  }

  const bool is_redirect_rule =
      parsed_rule.action.type == dnr_api::RuleActionType::kRedirect;

  if (is_redirect_rule) {
    if (!parsed_rule.action.redirect) {
      return ParseResult::ERROR_INVALID_REDIRECT;
    }

    ParseResult result = ParseRedirect(std::move(*parsed_rule.action.redirect),
                                       base_url, indexed_rule);
    if (result != ParseResult::SUCCESS) {
      return result;
    }
  }

  if (parsed_rule.condition.domains && parsed_rule.condition.domains->empty()) {
    return ParseResult::ERROR_EMPTY_DOMAINS_LIST;
  }

  if (parsed_rule.condition.initiator_domains &&
      parsed_rule.condition.initiator_domains->empty()) {
    return ParseResult::ERROR_EMPTY_INITIATOR_DOMAINS_LIST;
  }

  if (parsed_rule.condition.request_domains &&
      parsed_rule.condition.request_domains->empty()) {
    return ParseResult::ERROR_EMPTY_REQUEST_DOMAINS_LIST;
  }

  if (parsed_rule.condition.resource_types &&
      parsed_rule.condition.resource_types->empty()) {
    return ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST;
  }

  if (parsed_rule.condition.request_methods &&
      parsed_rule.condition.request_methods->empty()) {
    return ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST;
  }

  if (parsed_rule.condition.tab_ids && parsed_rule.condition.tab_ids->empty()) {
    return ParseResult::ERROR_EMPTY_TAB_IDS_LIST;
  }

  bool is_session_scoped_ruleset = ruleset_id == kSessionRulesetID;
  if (!is_session_scoped_ruleset && (parsed_rule.condition.tab_ids ||
                                     parsed_rule.condition.excluded_tab_ids)) {
    return ParseResult::ERROR_TAB_IDS_ON_NON_SESSION_RULE;
  }

  if (parsed_rule.condition.url_filter && parsed_rule.condition.regex_filter) {
    return ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED;
  }

  ParseResult validate_regex_result = ValidateRegex(
      IsCaseSensitive(parsed_rule), parsed_rule.condition.regex_filter,
      indexed_rule->regex_substitution);

  if (validate_regex_result != ParseResult::SUCCESS) {
    return validate_regex_result;
  }

  if (parsed_rule.condition.url_filter) {
    if (parsed_rule.condition.url_filter->empty()) {
      return ParseResult::ERROR_EMPTY_URL_FILTER;
    }

    if (!base::IsStringASCII(*parsed_rule.condition.url_filter)) {
      return ParseResult::ERROR_NON_ASCII_URL_FILTER;
    }
  }

  indexed_rule->action_type = parsed_rule.action.type;
  indexed_rule->id = base::checked_cast<uint32_t>(parsed_rule.id);
  indexed_rule->priority =
      ComputeIndexedRulePriority(priority, indexed_rule->action_type);
  indexed_rule->options = GetOptionsMask(parsed_rule);
  indexed_rule->activation_types = GetActivationTypes(parsed_rule);

  {
    ParseResult result =
        ComputeRequestMethods(parsed_rule, &indexed_rule->request_methods);
    if (result != ParseResult::SUCCESS) {
      return result;
    }
  }

  {
    ParseResult result =
        ComputeElementTypes(parsed_rule, &indexed_rule->element_types);
    if (result != ParseResult::SUCCESS) {
      return result;
    }
  }

  if (parsed_rule.condition.domains &&
      parsed_rule.condition.initiator_domains) {
    return ParseResult::ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED;
  }

  if (parsed_rule.condition.excluded_domains &&
      parsed_rule.condition.excluded_initiator_domains) {
    return ParseResult::
        ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED;
  }

  // Note: The `domains` and `excluded_domains` rule conditions are deprecated.
  //       If they are specified, they are mapped to the `initiator_domains` and
  //       `excluded_initiator_domains` conditions on the indexed rule.

  if (parsed_rule.condition.domains &&
      !CanonicalizeDomains(std::move(parsed_rule.condition.domains),
                           &indexed_rule->initiator_domains)) {
    return ParseResult::ERROR_NON_ASCII_DOMAIN;
  }

  if (parsed_rule.condition.initiator_domains &&
      !CanonicalizeDomains(std::move(parsed_rule.condition.initiator_domains),
                           &indexed_rule->initiator_domains)) {
    return ParseResult::ERROR_NON_ASCII_INITIATOR_DOMAIN;
  }

  if (parsed_rule.condition.excluded_domains &&
      !CanonicalizeDomains(std::move(parsed_rule.condition.excluded_domains),
                           &indexed_rule->excluded_initiator_domains)) {
    return ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN;
  }

  if (parsed_rule.condition.excluded_initiator_domains &&
      !CanonicalizeDomains(
          std::move(parsed_rule.condition.excluded_initiator_domains),
          &indexed_rule->excluded_initiator_domains)) {
    return ParseResult::ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN;
  }

  if (!CanonicalizeDomains(std::move(parsed_rule.condition.request_domains),
                           &indexed_rule->request_domains)) {
    return ParseResult::ERROR_NON_ASCII_REQUEST_DOMAIN;
  }

  if (!CanonicalizeDomains(
          std::move(parsed_rule.condition.excluded_request_domains),
          &indexed_rule->excluded_request_domains)) {
    return ParseResult::ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN;
  }

  {
    ParseTabIds(base::OptionalToPtr(parsed_rule.condition.tab_ids),
                indexed_rule->tab_ids);
    ParseTabIds(base::OptionalToPtr(parsed_rule.condition.excluded_tab_ids),
                indexed_rule->excluded_tab_ids);
    if (base::ranges::any_of(
            indexed_rule->tab_ids, [indexed_rule](int included_tab_id) {
              return base::Contains(indexed_rule->excluded_tab_ids,
                                    included_tab_id);
            })) {
      return ParseResult::ERROR_TAB_ID_DUPLICATED;
    }

    // When both `tab_ids` and `excluded_tab_ids` are populated, only the
    // included tab IDs are relevant.
    if (!indexed_rule->tab_ids.empty() &&
        !indexed_rule->excluded_tab_ids.empty()) {
      indexed_rule->excluded_tab_ids.clear();
    }
  }

  if (IsResponseHeaderMatchingEnabled()) {
    if (parsed_rule.condition.response_headers) {
      if (parsed_rule.condition.response_headers->empty()) {
        return ParseResult::ERROR_EMPTY_RESPONSE_HEADER_MATCHING_LIST;
      }

      indexed_rule->response_headers =
          std::move(*parsed_rule.condition.response_headers);
    }

    if (parsed_rule.condition.excluded_response_headers) {
      if (parsed_rule.condition.excluded_response_headers->empty()) {
        return ParseResult::ERROR_EMPTY_EXCLUDED_RESPONSE_HEADER_MATCHING_LIST;
      }

      indexed_rule->excluded_response_headers =
          std::move(*parsed_rule.condition.excluded_response_headers);
    }

    ParseResult result = ValidateResponseHeadersForMatching(
        indexed_rule->response_headers,
        indexed_rule->excluded_response_headers);
    if (result != ParseResult::SUCCESS) {
      return result;
    }
  }

  if (parsed_rule.condition.regex_filter.has_value()) {
    indexed_rule->url_pattern_type =
        url_pattern_index::flat::UrlPatternType_REGEXP;
    indexed_rule->url_pattern = std::move(*parsed_rule.condition.regex_filter);
  } else {
    // Parse the |anchor_left|, |anchor_right|, |url_pattern_type| and
    // |url_pattern| fields.
    UrlFilterParser::Parse(std::move(parsed_rule.condition.url_filter),
                           indexed_rule);
  }

  // url_pattern_index doesn't support patterns starting with a domain anchor
  // followed by a wildcard, e.g. ||*xyz.
  if (indexed_rule->anchor_left == flat_rule::AnchorType_SUBDOMAIN &&
      !indexed_rule->url_pattern.empty() &&
      indexed_rule->url_pattern.front() == kWildcardCharacter) {
    return ParseResult::ERROR_INVALID_URL_FILTER;
  }

  // Lower-case case-insensitive patterns as required by url pattern index.
  if (indexed_rule->options & flat_rule::OptionFlag_IS_CASE_INSENSITIVE) {
    indexed_rule->url_pattern = base::ToLowerASCII(indexed_rule->url_pattern);
  }

  if (parsed_rule.action.type == dnr_api::RuleActionType::kModifyHeaders) {
    if (!parsed_rule.action.request_headers &&
        !parsed_rule.action.response_headers) {
      return ParseResult::ERROR_NO_HEADERS_TO_MODIFY_SPECIFIED;
    }

    if (parsed_rule.action.request_headers) {
      if (!indexed_rule->response_headers.empty() ||
          !indexed_rule->excluded_response_headers.empty()) {
        return ParseResult::
            ERROR_RESPONSE_HEADER_RULE_CANNOT_MODIFY_REQUEST_HEADERS;
      }

      indexed_rule->request_headers_to_modify =
          std::move(*parsed_rule.action.request_headers);
      PopulateHeaderRegexOptions(indexed_rule->request_headers_to_modify);

      ParseResult result = ValidateHeadersForModification(
          indexed_rule->request_headers_to_modify,
          /*are_request_headers=*/true);
      if (result != ParseResult::SUCCESS) {
        return result;
      }
    }

    if (parsed_rule.action.response_headers) {
      indexed_rule->response_headers_to_modify =
          std::move(*parsed_rule.action.response_headers);
      PopulateHeaderRegexOptions(indexed_rule->response_headers_to_modify);

      ParseResult result = ValidateHeadersForModification(
          indexed_rule->response_headers_to_modify,
          /*are_request_headers=*/false);
      if (result != ParseResult::SUCCESS) {
        return result;
      }
    }
  }

  // Some sanity checks to ensure we return a valid IndexedRule.
  DCHECK_GE(indexed_rule->id, static_cast<uint32_t>(kMinValidID));
  DCHECK(IsSubset(indexed_rule->options, flat_rule::OptionFlag_ANY));
  DCHECK(IsSubset(indexed_rule->element_types, flat_rule::ElementType_ANY));
  DCHECK_EQ(flat_rule::ActivationType_NONE, indexed_rule->activation_types);
  DCHECK_NE(flat_rule::AnchorType_SUBDOMAIN, indexed_rule->anchor_right);

  return ParseResult::SUCCESS;
}

uint64_t ComputeIndexedRulePriority(int parsed_rule_priority,
                                    dnr_api::RuleActionType action_type) {
  // Incorporate the action's priority into the rule priority, so e.g. allow
  // rules will be given a higher priority than block rules with the same
  // priority specified in the rule JSON.
  return (base::checked_cast<uint32_t>(parsed_rule_priority) << 8) |
         GetActionTypePriority(action_type);
}

}  // namespace extensions::declarative_net_request
