// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/test_utils.h"

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

namespace extensions {
namespace keys = manifest_keys;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

namespace {

const base::FilePath::CharType kBackgroundScriptFilepath[] =
    FILE_PATH_LITERAL("background.js");

std::unique_ptr<base::Value> ToValue(const std::string& t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(int t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(bool t) {
  return std::make_unique<base::Value>(t);
}

std::unique_ptr<base::Value> ToValue(const DictionarySource& source) {
  return source.ToValue();
}

std::unique_ptr<base::Value> ToValue(const TestRulesetInfo& info) {
  return info.GetManifestValue();
}

template <typename T>
std::unique_ptr<base::ListValue> ToValue(const std::vector<T>& vec) {
  ListBuilder builder;
  for (const T& t : vec)
    builder.Append(ToValue(t));
  return builder.Build();
}

template <typename T>
void SetValue(base::DictionaryValue* dict,
              const char* key,
              const absl::optional<T>& value) {
  if (!value)
    return;

  dict->SetKey(key, base::Value::FromUniquePtrValue(ToValue(*value)));
}

}  // namespace

TestRuleCondition::TestRuleCondition() = default;
TestRuleCondition::~TestRuleCondition() = default;
TestRuleCondition::TestRuleCondition(const TestRuleCondition&) = default;
TestRuleCondition& TestRuleCondition::operator=(const TestRuleCondition&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleCondition::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kUrlFilterKey, url_filter);
  SetValue(dict.get(), kRegexFilterKey, regex_filter);
  SetValue(dict.get(), kIsUrlFilterCaseSensitiveKey,
           is_url_filter_case_sensitive);
  SetValue(dict.get(), kDomainsKey, domains);
  SetValue(dict.get(), kExcludedDomainsKey, excluded_domains);
  SetValue(dict.get(), kInitiatorDomainsKey, initiator_domains);
  SetValue(dict.get(), kExcludedInitiatorDomainsKey,
           excluded_initiator_domains);
  SetValue(dict.get(), kRequestDomainsKey, request_domains);
  SetValue(dict.get(), kExcludedRequestDomainsKey, excluded_request_domains);
  SetValue(dict.get(), kRequestMethodsKey, request_methods);
  SetValue(dict.get(), kExcludedRequestMethodsKey, excluded_request_methods);
  SetValue(dict.get(), kResourceTypesKey, resource_types);
  SetValue(dict.get(), kExcludedResourceTypesKey, excluded_resource_types);
  SetValue(dict.get(), kTabIdsKey, tab_ids);
  SetValue(dict.get(), kExcludedTabIdsKey, excluded_tab_ids);
  SetValue(dict.get(), kDomainTypeKey, domain_type);

  return dict;
}

TestRuleQueryKeyValue::TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::~TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::TestRuleQueryKeyValue(const TestRuleQueryKeyValue&) =
    default;
TestRuleQueryKeyValue& TestRuleQueryKeyValue::operator=(
    const TestRuleQueryKeyValue&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleQueryKeyValue::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kQueryKeyKey, key);
  SetValue(dict.get(), kQueryValueKey, value);
  SetValue(dict.get(), kQueryReplaceOnlyKey, replace_only);
  return dict;
}

TestRuleQueryTransform::TestRuleQueryTransform() = default;
TestRuleQueryTransform::~TestRuleQueryTransform() = default;
TestRuleQueryTransform::TestRuleQueryTransform(const TestRuleQueryTransform&) =
    default;
TestRuleQueryTransform& TestRuleQueryTransform::operator=(
    const TestRuleQueryTransform&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleQueryTransform::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kQueryTransformRemoveParamsKey, remove_params);
  SetValue(dict.get(), kQueryTransformAddReplaceParamsKey,
           add_or_replace_params);
  return dict;
}

TestRuleTransform::TestRuleTransform() = default;
TestRuleTransform::~TestRuleTransform() = default;
TestRuleTransform::TestRuleTransform(const TestRuleTransform&) = default;
TestRuleTransform& TestRuleTransform::operator=(const TestRuleTransform&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleTransform::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kTransformSchemeKey, scheme);
  SetValue(dict.get(), kTransformHostKey, host);
  SetValue(dict.get(), kTransformPortKey, port);
  SetValue(dict.get(), kTransformPathKey, path);
  SetValue(dict.get(), kTransformQueryKey, query);
  SetValue(dict.get(), kTransformQueryTransformKey, query_transform);
  SetValue(dict.get(), kTransformFragmentKey, fragment);
  SetValue(dict.get(), kTransformUsernameKey, username);
  SetValue(dict.get(), kTransformPasswordKey, password);
  return dict;
}

TestRuleRedirect::TestRuleRedirect() = default;
TestRuleRedirect::~TestRuleRedirect() = default;
TestRuleRedirect::TestRuleRedirect(const TestRuleRedirect&) = default;
TestRuleRedirect& TestRuleRedirect::operator=(const TestRuleRedirect&) =
    default;

std::unique_ptr<base::DictionaryValue> TestRuleRedirect::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kExtensionPathKey, extension_path);
  SetValue(dict.get(), kTransformKey, transform);
  SetValue(dict.get(), kRedirectUrlKey, url);
  SetValue(dict.get(), kRegexSubstitutionKey, regex_substitution);
  return dict;
}

TestHeaderInfo::TestHeaderInfo(std::string header,
                               std::string operation,
                               absl::optional<std::string> value)
    : header(std::move(header)),
      operation(std::move(operation)),
      value(std::move(value)) {}
TestHeaderInfo::~TestHeaderInfo() = default;
TestHeaderInfo::TestHeaderInfo(const TestHeaderInfo&) = default;
TestHeaderInfo& TestHeaderInfo::operator=(const TestHeaderInfo&) = default;

std::unique_ptr<base::DictionaryValue> TestHeaderInfo::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kHeaderNameKey, header);
  SetValue(dict.get(), kHeaderOperationKey, operation);
  SetValue(dict.get(), kHeaderValueKey, value);
  return dict;
}

TestRuleAction::TestRuleAction() = default;
TestRuleAction::~TestRuleAction() = default;
TestRuleAction::TestRuleAction(const TestRuleAction&) = default;
TestRuleAction& TestRuleAction::operator=(const TestRuleAction&) = default;

std::unique_ptr<base::DictionaryValue> TestRuleAction::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kRuleActionTypeKey, type);
  SetValue(dict.get(), kRequestHeadersKey, request_headers);
  SetValue(dict.get(), kResponseHeadersKey, response_headers);
  SetValue(dict.get(), kRedirectKey, redirect);
  return dict;
}

TestRule::TestRule() = default;
TestRule::~TestRule() = default;
TestRule::TestRule(const TestRule&) = default;
TestRule& TestRule::operator=(const TestRule&) = default;

std::unique_ptr<base::DictionaryValue> TestRule::ToValue() const {
  auto dict = std::make_unique<base::DictionaryValue>();
  SetValue(dict.get(), kIDKey, id);
  SetValue(dict.get(), kPriorityKey, priority);
  SetValue(dict.get(), kRuleConditionKey, condition);
  SetValue(dict.get(), kRuleActionKey, action);
  return dict;
}

TestRule CreateGenericRule(int id) {
  TestRuleCondition condition;
  condition.url_filter = std::string("filter");
  TestRuleAction action;
  action.type = std::string("block");
  TestRule rule;
  rule.id = id;
  rule.priority = kMinValidPriority;
  rule.action = action;
  rule.condition = condition;
  return rule;
}

TestRule CreateRegexRule(int id) {
  TestRule rule = CreateGenericRule(id);
  rule.condition->url_filter.reset();
  rule.condition->regex_filter = std::string("filter");
  return rule;
}

TestRulesetInfo::TestRulesetInfo(const std::string& manifest_id_and_path,
                                 const base::Value& rules_value,
                                 bool enabled)
    : TestRulesetInfo(manifest_id_and_path,
                      manifest_id_and_path,
                      rules_value,
                      enabled) {}

TestRulesetInfo::TestRulesetInfo(const std::string& manifest_id,
                                 const std::string& relative_file_path,
                                 const base::Value& rules_value,
                                 bool enabled)
    : manifest_id(manifest_id),
      relative_file_path(relative_file_path),
      rules_value(rules_value.Clone()),
      enabled(enabled) {}

TestRulesetInfo::TestRulesetInfo(const TestRulesetInfo& info)
    : TestRulesetInfo(info.manifest_id,
                      info.relative_file_path,
                      info.rules_value,
                      info.enabled) {}

std::unique_ptr<base::DictionaryValue> TestRulesetInfo::GetManifestValue()
    const {
  dnr_api::Ruleset ruleset;
  ruleset.id = manifest_id;
  ruleset.path = relative_file_path;
  ruleset.enabled = enabled;
  return ruleset.ToValue();
}

std::unique_ptr<base::DictionaryValue> CreateManifest(
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags,
    const std::string& extension_name) {
  std::vector<std::string> permissions = hosts;

  if (!(flags & kConfig_OmitDeclarativeNetRequestPermission))
    permissions.push_back(kDeclarativeNetRequestPermission);

  // These permissions are needed for some tests. TODO(karandeepb): Add a
  // ConfigFlag for these.
  permissions.push_back("webRequest");
  permissions.push_back("webRequestBlocking");

  if (flags & kConfig_HasFeedbackPermission)
    permissions.push_back(kFeedbackAPIPermission);

  if (flags & kConfig_HasActiveTab)
    permissions.push_back("activeTab");

  if (flags & kConfig_HasDelarativeNetRequestWithHostAccessPermission)
    permissions.push_back("declarativeNetRequestWithHostAccess");

  std::vector<std::string> background_scripts;
  if (flags & kConfig_HasBackgroundScript)
    background_scripts.push_back("background.js");

  DictionaryBuilder manifest_builder;

  if (flags & kConfig_OmitDeclarativeNetRequestKey) {
    DCHECK(ruleset_info.empty());
  } else {
    manifest_builder.Set(
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        DictionaryBuilder()
            .Set(dnr_api::DNRInfo::kRuleResources, ToValue(ruleset_info))
            .Build());
  }

  return manifest_builder.Set(keys::kName, extension_name)
      .Set(keys::kPermissions, ToValue(permissions))
      .Set(keys::kVersion, "1.0")
      .Set(keys::kManifestVersion, 2)
      .Set("background", DictionaryBuilder()
                             .Set("scripts", ToValue(background_scripts))
                             .Build())
      .Set(keys::kBrowserAction, DictionaryBuilder().Build())
      .Build();
}

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& vec) {
  return ToValue(vec);
}

std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<TestRule>& rules) {
  return ToValue(rules);
}

void WriteManifestAndRulesets(const base::FilePath& extension_dir,
                              const std::vector<TestRulesetInfo>& ruleset_info,
                              const std::vector<std::string>& hosts,
                              unsigned flags,
                              const std::string& extension_name) {
  // Persist JSON rules files.
  for (const TestRulesetInfo& info : ruleset_info) {
    JSONFileValueSerializer(extension_dir.AppendASCII(info.relative_file_path))
        .Serialize(info.rules_value);
  }

  // Persists a background script if needed.
  if (flags & ConfigFlag::kConfig_HasBackgroundScript) {
    std::string content = "chrome.test.sendMessage('ready');";
    CHECK_EQ(static_cast<int>(content.length()),
             base::WriteFile(extension_dir.Append(kBackgroundScriptFilepath),
                             content.c_str(), content.length()));
  }

  // Persist manifest file.
  JSONFileValueSerializer(extension_dir.Append(kManifestFilename))
      .Serialize(*CreateManifest(ruleset_info, hosts, flags, extension_name));
}

void WriteManifestAndRuleset(const base::FilePath& extension_dir,
                             const TestRulesetInfo& info,
                             const std::vector<std::string>& hosts,
                             unsigned flags,
                             const std::string& extension_name) {
  WriteManifestAndRulesets(extension_dir, {info}, hosts, flags, extension_name);
}

}  // namespace declarative_net_request
}  // namespace extensions
