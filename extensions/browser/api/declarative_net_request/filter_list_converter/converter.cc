// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "url/gurl.h"

namespace extensions::declarative_net_request {

namespace {

namespace proto = ::url_pattern_index::proto;
namespace dnr_api = extensions::api::declarative_net_request;

using ElementTypeMap =
    base::flat_map<proto::ElementType, dnr_api::ResourceType>;

// Utility class to convert the proto::UrlRule format to the JSON format
// supported by Declarative Net Request.
class ProtoToJSONRuleConverter {
 public:
  ProtoToJSONRuleConverter(const ProtoToJSONRuleConverter&) = delete;
  ProtoToJSONRuleConverter& operator=(const ProtoToJSONRuleConverter&) = delete;

  // Returns a dictionary value corresponding to a Declarative Net Request rule
  // on success. On error, returns an empty/null value and populates |error|.
  // |error| must be non-null.
  static base::Value Convert(const proto::UrlRule& rule,
                             int rule_id,
                             std::string* error) {
    CHECK(error);
    ProtoToJSONRuleConverter json_rule(rule, rule_id);
    return json_rule.Convert(error);
  }

 private:
  ProtoToJSONRuleConverter(const proto::UrlRule& rule, int rule_id)
      : input_rule_(rule), rule_id_(rule_id) {}

  base::Value Convert(std::string* error) {
    CHECK(error);

    // Populate all the keys.
    bool success = CheckActivationType() && PopulateID() &&
                   PopulatePriorirty() && PopulateURLFilter() &&
                   PopulateIsURLFilterCaseSensitive() && PopulateDomains() &&
                   PopulateExcludedDomains() && PopulateResourceTypes() &&
                   PopulateExcludedResourceTypes() && PopulateDomainType() &&
                   PopulateRuleActionType() && PopulateRedirectURL() &&
                   PopulateRemoveHeadersList();

    if (!success) {
      CHECK(!error_.empty());
      *error = std::move(error_);
      return base::Value();
    }

    // Sanity check that we can parse this rule.
    base::Value json_rule(std::move(json_rule_));
    auto rule = dnr_api::Rule::FromValue(json_rule);
    CHECK(rule.has_value())
        << "Converted rule can't be parsed. Error: " << rule.error()
        << json_rule;

    IndexedRule indexed_rule;
    ParseResult result = IndexedRule::CreateIndexedRule(
        std::move(rule).value(), GURL() /* base_url */,
        kMinValidStaticRulesetID, &indexed_rule);

    auto get_non_ascii_error = [this](const std::string& context) {
      return base::StringPrintf(
          "Rule with filter '%s' ignored due to non ascii characters in %s.",
          input_rule_.url_pattern().c_str(), context.c_str());
    };

    // Non-ascii characters in rules are not supported.
    if (result == ParseResult::ERROR_NON_ASCII_URL_FILTER) {
      *error = get_non_ascii_error("url filter");
      return base::Value();
    }
    if (result == ParseResult::ERROR_NON_ASCII_DOMAIN) {
      *error = get_non_ascii_error("domains");
      return base::Value();
    }
    if (result == ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN) {
      *error = get_non_ascii_error("excluded domains");
      return base::Value();
    }

    CHECK_EQ(ParseResult::SUCCESS, result)
        << "Unexpected parse error << " << static_cast<int>(result)
        << " for rule " << json_rule;

    return json_rule;
  }

  bool CheckActivationType() {
    if (input_rule_.activation_types() == proto::ACTIVATION_TYPE_UNSPECIFIED) {
      return true;
    }

    if (input_rule_.activation_types() == proto::ACTIVATION_TYPE_DOCUMENT) {
      is_allow_all_requests_rule_ = true;
      return true;
    }

    std::vector<std::string> activation_types;
    for (int activation_type = 1; activation_type <= proto::ACTIVATION_TYPE_MAX;
         activation_type <<= 1) {
      CHECK(proto::ActivationType_IsValid(activation_type));
      if (!(input_rule_.activation_types() & activation_type)) {
        continue;
      }

      switch (static_cast<proto::ActivationType>(activation_type)) {
        case proto::ACTIVATION_TYPE_UNSPECIFIED:
          CHECK(false);
          break;
        case proto::ACTIVATION_TYPE_DOCUMENT:
          activation_types.emplace_back("document");
          break;
        case proto::ACTIVATION_TYPE_ELEMHIDE:
          activation_types.emplace_back("elemhide");
          break;
        case proto::ACTIVATION_TYPE_GENERICHIDE:
          activation_types.emplace_back("generichide");
          break;
        case proto::ACTIVATION_TYPE_GENERICBLOCK:
          activation_types.emplace_back("genericblock");
          break;
        case proto::ACTIVATION_TYPE_ALL:
          CHECK(false);
          break;
      }
    }

    // We don't support any activation types.
    error_ = base::StringPrintf(
        "Rule with filter '%s' ignored due to invalid activation types-[%s].",
        input_rule_.url_pattern().c_str(),
        base::JoinString(activation_types, "," /* separator */).c_str());
    return false;
  }

  bool PopulateID() {
    CHECK_GE(rule_id_, kMinValidID);
    CHECK(json_rule_.Set(kIDKey, rule_id_));
    return true;
  }

  bool PopulatePriorirty() {
    CHECK(json_rule_.Set(kPriorityKey, kMinValidPriority));
    return true;
  }

  bool PopulateURLFilter() {
    // Pattern type validation.
    CHECK_NE(proto::URL_PATTERN_TYPE_UNSPECIFIED,
             input_rule_.url_pattern_type());

    // TODO(karandeepb): It would be nice to print the actual filter-list string
    // in cases where rule conversion fails.
    if (input_rule_.url_pattern_type() == proto::URL_PATTERN_TYPE_REGEXP) {
      error_ = base::StringPrintf(
          "Rule with filter %s ignored since regex rules are not supported.",
          input_rule_.url_pattern().c_str());
      return false;
    }

    std::string result;
    switch (input_rule_.anchor_left()) {
      case proto::ANCHOR_TYPE_NONE:
        break;
      case proto::ANCHOR_TYPE_BOUNDARY:
        result += '|';
        break;
      case proto::ANCHOR_TYPE_SUBDOMAIN:
        result += "||";
        break;
      case proto::ANCHOR_TYPE_UNSPECIFIED:
        CHECK(false);
        break;
    }

    result += input_rule_.url_pattern();

    switch (input_rule_.anchor_right()) {
      case proto::ANCHOR_TYPE_NONE:
        break;
      case proto::ANCHOR_TYPE_BOUNDARY:
        result += '|';
        break;
      case proto::ANCHOR_TYPE_SUBDOMAIN:
      case proto::ANCHOR_TYPE_UNSPECIFIED:
        CHECK(false);
        break;
    }

    // If |result| is empty, omit persisting the url pattern. In that case, it
    // will match all urls.
    if (!result.empty()) {
      CHECK(
          json_rule_.EnsureDict(kRuleConditionKey)->Set(kUrlFilterKey, result));
    }

    return true;
  }

  bool PopulateIsURLFilterCaseSensitive() {
    // Omit if case sensitive, since it's the default.
    const bool case_sensitive = input_rule_.match_case();
    if (case_sensitive) {
      return true;
    }

    CHECK(json_rule_.EnsureDict(kRuleConditionKey)
              ->Set(kIsUrlFilterCaseSensitiveKey, false));
    return true;
  }

  bool PopulateDomains() {
    return PopulateDomainsInternal(kDomainsKey, false /*exclude_value*/);
  }

  bool PopulateExcludedDomains() {
    return PopulateDomainsInternal(kExcludedDomainsKey, true /*exclude_value*/);
  }

  bool PopulateDomainsInternal(std::string_view sub_key, bool exclude_value) {
    base::Value::List domains;

    // Note: This isn't always correct. Filters consider the $domain option to
    //       match the request domain for main_frame requests - not the
    //       initiator domain.
    for (const proto::DomainListItem& item : input_rule_.initiator_domains()) {
      if (item.exclude() == exclude_value) {
        domains.Append(item.domain());
      }
    }

    // Omit empty domain list.
    if (!domains.empty()) {
      CHECK(json_rule_.EnsureDict(kRuleConditionKey)
                ->Set(sub_key, std::move(domains)));
    }

    return true;
  }

  base::Value::List GetResourceTypeList(int element_mask) {
    base::Value::List resource_types;
    for (int element_type = 1; element_type <= proto::ElementType_MAX;
         element_type <<= 1) {
      CHECK(proto::ElementType_IsValid(element_type));

      if (!(element_type & element_mask)) {
        continue;
      }

      dnr_api::ResourceType resource_type = dnr_api::ResourceType::kNone;
      switch (static_cast<proto::ElementType>(element_type)) {
        case proto::ELEMENT_TYPE_UNSPECIFIED:
          CHECK(false);
          break;
        case proto::ELEMENT_TYPE_OTHER:
          resource_type = dnr_api::ResourceType::kOther;
          break;
        case proto::ELEMENT_TYPE_SCRIPT:
          resource_type = dnr_api::ResourceType::kScript;
          break;
        case proto::ELEMENT_TYPE_IMAGE:
          resource_type = dnr_api::ResourceType::kImage;
          break;
        case proto::ELEMENT_TYPE_STYLESHEET:
          resource_type = dnr_api::ResourceType::kStylesheet;
          break;
        case proto::ELEMENT_TYPE_OBJECT:
          resource_type = dnr_api::ResourceType::kObject;
          break;
        case proto::ELEMENT_TYPE_XMLHTTPREQUEST:
          resource_type = dnr_api::ResourceType::kXmlhttprequest;
          break;
        case proto::ELEMENT_TYPE_OBJECT_SUBREQUEST:
          CHECK(false);
          break;
        case proto::ELEMENT_TYPE_SUBDOCUMENT:
          resource_type = dnr_api::ResourceType::kSubFrame;
          break;
        case proto::ELEMENT_TYPE_PING:
          resource_type = dnr_api::ResourceType::kPing;
          break;
        case proto::ELEMENT_TYPE_MEDIA:
          resource_type = dnr_api::ResourceType::kMedia;
          break;
        case proto::ELEMENT_TYPE_FONT:
          resource_type = dnr_api::ResourceType::kFont;
          break;
        case proto::ELEMENT_TYPE_POPUP:
          CHECK(false);
          break;
        case proto::ELEMENT_TYPE_WEBSOCKET:
          resource_type = dnr_api::ResourceType::kWebsocket;
          break;
        case proto::ELEMENT_TYPE_WEBTRANSPORT:
          resource_type = dnr_api::ResourceType::kWebtransport;
          break;
        case proto::ELEMENT_TYPE_WEBBUNDLE:
          resource_type = dnr_api::ResourceType::kWebbundle;
          break;
        case proto::ELEMENT_TYPE_ALL:
          CHECK(false);
          break;
      }

      resource_types.Append(dnr_api::ToString(resource_type));
    }

    return resource_types;
  }

  bool PopulateResourceTypes() {
    // Ensure that |element_types()| is a subset of proto::ElementType_ALL.
    CHECK_EQ(proto::ELEMENT_TYPE_ALL,
             proto::ELEMENT_TYPE_ALL | input_rule_.element_types());

    int kMaskUnsupported =
        proto::ELEMENT_TYPE_POPUP | proto::ELEMENT_TYPE_OBJECT_SUBREQUEST;

    int element_mask = input_rule_.element_types() & (~kMaskUnsupported);

    // We don't support object-subrequest. Instead let these be treated as rules
    // matching object requests.
    if (input_rule_.element_types() & proto::ELEMENT_TYPE_OBJECT_SUBREQUEST) {
      element_mask |= proto::ELEMENT_TYPE_OBJECT;
    }

    if (is_allow_all_requests_rule_) {
      // Any subresource types specified with ACTIVATION_TYPE_DOCUMENT are
      // invalid.
      if (element_mask && element_mask != proto::ELEMENT_TYPE_SUBDOCUMENT) {
        std::stringstream error_stream;
        error_stream << "$document rule with filter "
                     << input_rule_.url_pattern()
                     << " ignored. Invalid resource types: "
                     << GetResourceTypeList(element_mask);
        error_ = error_stream.str();
        return false;
      }
    } else if (!element_mask) {  // No supported element types.
      const char* ignored_types =
          input_rule_.element_types() & proto::ELEMENT_TYPE_POPUP ? "popup"
                                                                  : "";

      error_ = base::StringPrintf(
          "Rule with filter %s and resource types [%s] ignored: No applicable "
          "resource types",
          input_rule_.url_pattern().c_str(), ignored_types);
      return false;
    }

    // Omit resource types to block all subresources by default.
    if (element_mask == (proto::ELEMENT_TYPE_ALL & ~kMaskUnsupported)) {
      return true;
    }

    base::Value::List resource_types = GetResourceTypeList(element_mask);
    if (is_allow_all_requests_rule_) {
      resource_types.Append(
          dnr_api::ToString(dnr_api::ResourceType::kMainFrame));
    }

    CHECK(json_rule_.EnsureDict(kRuleConditionKey)
              ->Set(kResourceTypesKey, std::move(resource_types)));
    return true;
  }

  bool PopulateExcludedResourceTypes() {
    // We don't populate the "excludedResourceTypes" since that information has
    // been processed away by conversion to a proto::UrlRule.
    return true;
  }

  bool PopulateDomainType() {
    dnr_api::DomainType domain_type = dnr_api::DomainType::kNone;

    switch (input_rule_.source_type()) {
      case proto::SOURCE_TYPE_ANY:
        // This is the default domain type and can be omitted.
        return true;
      case proto::SOURCE_TYPE_FIRST_PARTY:
        domain_type = dnr_api::DomainType::kFirstParty;
        break;
      case proto::SOURCE_TYPE_THIRD_PARTY:
        domain_type = dnr_api::DomainType::kThirdParty;
        break;
      case proto::SOURCE_TYPE_UNSPECIFIED:
        CHECK(false);
        break;
    }

    CHECK_NE(dnr_api::DomainType::kNone, domain_type);
    CHECK(json_rule_.EnsureDict(kRuleConditionKey)
              ->Set(kDomainTypeKey, dnr_api::ToString(domain_type)));
    return true;
  }

  bool PopulateRuleActionType() {
    dnr_api::RuleActionType action_type = dnr_api::RuleActionType::kNone;

    CHECK(!is_allow_all_requests_rule_ ||
          input_rule_.semantics() == proto::RULE_SEMANTICS_ALLOWLIST);

    switch (input_rule_.semantics()) {
      case proto::RULE_SEMANTICS_BLOCKLIST:
        action_type = dnr_api::RuleActionType::kBlock;
        break;
      case proto::RULE_SEMANTICS_ALLOWLIST:
        if (is_allow_all_requests_rule_) {
          action_type = dnr_api::RuleActionType::kAllowAllRequests;
        } else {
          action_type = dnr_api::RuleActionType::kAllow;
        }
        break;
      case proto::RULE_SEMANTICS_UNSPECIFIED:
        CHECK(false);
        break;
    }

    CHECK_NE(dnr_api::RuleActionType::kNone, action_type);
    CHECK(json_rule_.EnsureDict(kRuleActionKey)
              ->Set(kRuleActionTypeKey, dnr_api::ToString(action_type)));
    return true;
  }

  bool PopulateRedirectURL() {
    // Do nothing. The tool only supports allow and block rules.
    return true;
  }

  bool PopulateRemoveHeadersList() {
    // Do nothing. The tool only supports allow and block rules.
    return true;
  }

  bool is_allow_all_requests_rule_ = false;
  proto::UrlRule input_rule_;
  int rule_id_;
  std::string error_;
  base::Value::Dict json_rule_;
};

// Writes rules/extension to |output_path| in the format supported by
// Declarative Net Request.
class DNRJsonRuleOutputStream : public subresource_filter::RuleOutputStream {
 public:
  DNRJsonRuleOutputStream(const base::FilePath& output_path,
                          filter_list_converter::WriteType type,
                          bool noisy)
      : rule_id_(kMinValidID),
        output_path_(output_path),
        write_type_(type),
        noisy_(noisy) {}

  DNRJsonRuleOutputStream(const DNRJsonRuleOutputStream&) = delete;
  DNRJsonRuleOutputStream& operator=(const DNRJsonRuleOutputStream&) = delete;

  bool PutUrlRule(const proto::UrlRule& rule) override {
    std::string error;
    base::Value json_rule_value =
        ProtoToJSONRuleConverter::Convert(rule, rule_id_, &error);

    if (json_rule_value.is_none()) {
      if (noisy_) {
        LOG(ERROR) << base::StringPrintf("Error for id %d: %s", rule_id_,
                                         error.c_str());
      }
      return false;
    }

    CHECK(error.empty());
    CHECK(json_rule_value.is_dict());
    output_rules_list_.Append(std::move(json_rule_value));
    ++rule_id_;
    return true;
  }

  bool PutCssRule(const proto::CssRule& rule) override {
    // Ignore CSS rules.
    return true;
  }

  bool Finish() override {
    constexpr char kJSONRulesFilename[] = "rules.json";
    constexpr char kRulesetID[] = "filter_list";

    switch (write_type_) {
      case filter_list_converter::kExtension: {
        TestRulesetInfo info(kRulesetID, kJSONRulesFilename,
                             output_rules_list_.Clone());
        WriteManifestAndRuleset(output_path_, info, {} /* hosts */);
        break;
      }
      case filter_list_converter::kJSONRuleset:
        JSONFileValueSerializer(output_path_).Serialize(output_rules_list_);
        break;
    }

    return true;
  }

 private:
  int rule_id_ = kMinValidID;
  base::Value::List output_rules_list_;
  const base::FilePath output_path_;
  const filter_list_converter::WriteType write_type_;
  const bool noisy_;
};

}  // namespace

namespace filter_list_converter {

bool ConvertRuleset(const std::vector<base::FilePath>& filter_list_inputs,
                    const base::FilePath& output_path,
                    WriteType type,
                    bool noisy) {
  DNRJsonRuleOutputStream rule_output_stream(output_path, type, noisy);

  for (const auto& input_path : filter_list_inputs) {
    auto rule_input_stream = subresource_filter::RuleInputStream::Create(
        std::make_unique<std::ifstream>(input_path.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::in),
        subresource_filter::RulesetFormat::kFilterList);
    CHECK(rule_input_stream);
    CHECK(subresource_filter::TransferRules(rule_input_stream.get(),
                                            &rule_output_stream,
                                            nullptr /* css_rule_output */));
  }

  return rule_output_stream.Finish();
}

}  // namespace filter_list_converter
}  // namespace extensions::declarative_net_request
