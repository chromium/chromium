// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/test_utils.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace dnr_api = api::declarative_net_request;

namespace declarative_net_request {

namespace {

const base::FilePath::CharType kBackgroundScriptFilepath[] =
    FILE_PATH_LITERAL("background.js");

base::Value ToValue(const std::string& t) {
  return base::Value(t);
}

base::Value ToValue(int t) {
  return base::Value(t);
}

base::Value ToValue(bool t) {
  return base::Value(t);
}

base::Value ToValue(const DictionarySource& source) {
  return base::Value(source.ToValue());
}

base::Value ToValue(const TestRulesetInfo& info) {
  return base::Value(info.GetManifestValue());
}

template <typename T>
base::Value::List ToValue(const std::vector<T>& vec) {
  base::Value::List builder;
  for (const T& t : vec)
    builder.Append(ToValue(t));
  return builder;
}

template <typename T>
void SetValue(base::Value::Dict& dict,
              const char* key,
              const std::optional<T>& value) {
  if (!value)
    return;

  dict.Set(key, ToValue(*value));
}

}  // namespace

TestRuleCondition::TestRuleCondition() = default;
TestRuleCondition::~TestRuleCondition() = default;
TestRuleCondition::TestRuleCondition(const TestRuleCondition&) = default;
TestRuleCondition& TestRuleCondition::operator=(const TestRuleCondition&) =
    default;

base::Value::Dict TestRuleCondition::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kUrlFilterKey, url_filter);
  SetValue(dict, kRegexFilterKey, regex_filter);
  SetValue(dict, kIsUrlFilterCaseSensitiveKey, is_url_filter_case_sensitive);
  SetValue(dict, kDomainsKey, domains);
  SetValue(dict, kExcludedDomainsKey, excluded_domains);
  SetValue(dict, kInitiatorDomainsKey, initiator_domains);
  SetValue(dict, kExcludedInitiatorDomainsKey, excluded_initiator_domains);
  SetValue(dict, kRequestDomainsKey, request_domains);
  SetValue(dict, kExcludedRequestDomainsKey, excluded_request_domains);
  SetValue(dict, kRequestMethodsKey, request_methods);
  SetValue(dict, kExcludedRequestMethodsKey, excluded_request_methods);
  SetValue(dict, kResourceTypesKey, resource_types);
  SetValue(dict, kExcludedResourceTypesKey, excluded_resource_types);
  SetValue(dict, kTabIdsKey, tab_ids);
  SetValue(dict, kExcludedTabIdsKey, excluded_tab_ids);
  SetValue(dict, kDomainTypeKey, domain_type);
  SetValue(dict, kResponseHeadersKey, response_headers);
  SetValue(dict, kExcludedResponseHeadersKey, excluded_response_headers);

  return dict;
}

TestRuleQueryKeyValue::TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::~TestRuleQueryKeyValue() = default;
TestRuleQueryKeyValue::TestRuleQueryKeyValue(const TestRuleQueryKeyValue&) =
    default;
TestRuleQueryKeyValue& TestRuleQueryKeyValue::operator=(
    const TestRuleQueryKeyValue&) = default;

base::Value::Dict TestRuleQueryKeyValue::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kQueryKeyKey, key);
  SetValue(dict, kQueryValueKey, value);
  SetValue(dict, kQueryReplaceOnlyKey, replace_only);
  return dict;
}

TestRuleQueryTransform::TestRuleQueryTransform() = default;
TestRuleQueryTransform::~TestRuleQueryTransform() = default;
TestRuleQueryTransform::TestRuleQueryTransform(const TestRuleQueryTransform&) =
    default;
TestRuleQueryTransform& TestRuleQueryTransform::operator=(
    const TestRuleQueryTransform&) = default;

base::Value::Dict TestRuleQueryTransform::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kQueryTransformRemoveParamsKey, remove_params);
  SetValue(dict, kQueryTransformAddReplaceParamsKey, add_or_replace_params);
  return dict;
}

TestRuleTransform::TestRuleTransform() = default;
TestRuleTransform::~TestRuleTransform() = default;
TestRuleTransform::TestRuleTransform(const TestRuleTransform&) = default;
TestRuleTransform& TestRuleTransform::operator=(const TestRuleTransform&) =
    default;

base::Value::Dict TestRuleTransform::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kTransformSchemeKey, scheme);
  SetValue(dict, kTransformHostKey, host);
  SetValue(dict, kTransformPortKey, port);
  SetValue(dict, kTransformPathKey, path);
  SetValue(dict, kTransformQueryKey, query);
  SetValue(dict, kTransformQueryTransformKey, query_transform);
  SetValue(dict, kTransformFragmentKey, fragment);
  SetValue(dict, kTransformUsernameKey, username);
  SetValue(dict, kTransformPasswordKey, password);
  return dict;
}

TestRuleRedirect::TestRuleRedirect() = default;
TestRuleRedirect::~TestRuleRedirect() = default;
TestRuleRedirect::TestRuleRedirect(const TestRuleRedirect&) = default;
TestRuleRedirect& TestRuleRedirect::operator=(const TestRuleRedirect&) =
    default;

base::Value::Dict TestRuleRedirect::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kExtensionPathKey, extension_path);
  SetValue(dict, kTransformKey, transform);
  SetValue(dict, kRedirectUrlKey, url);
  SetValue(dict, kRegexSubstitutionKey, regex_substitution);
  return dict;
}

TestHeaderInfo::TestHeaderInfo(std::string header,
                               std::string operation,
                               std::optional<std::string> value)
    : header(std::move(header)),
      operation(std::move(operation)),
      value(std::move(value)) {}
TestHeaderInfo::~TestHeaderInfo() = default;
TestHeaderInfo::TestHeaderInfo(const TestHeaderInfo&) = default;
TestHeaderInfo& TestHeaderInfo::operator=(const TestHeaderInfo&) = default;

base::Value::Dict TestHeaderInfo::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kHeaderNameKey, header);
  SetValue(dict, kHeaderOperationKey, operation);
  SetValue(dict, kHeaderValueKey, value);
  return dict;
}

TestHeaderCondition::TestHeaderCondition(
    std::string header,
    std::vector<std::string> values,
    std::vector<std::string> excluded_values)
    : header(std::move(header)),
      values(std::move(values)),
      excluded_values(std::move(excluded_values)) {}
TestHeaderCondition::~TestHeaderCondition() = default;
TestHeaderCondition::TestHeaderCondition(const TestHeaderCondition&) = default;
TestHeaderCondition& TestHeaderCondition::operator=(
    const TestHeaderCondition&) = default;

base::Value::Dict TestHeaderCondition::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kHeaderNameKey, header);
  SetValue(dict, kHeaderValuesKey, values);
  SetValue(dict, kHeaderExcludedValuesKey, excluded_values);
  return dict;
}

TestRuleAction::TestRuleAction() = default;
TestRuleAction::~TestRuleAction() = default;
TestRuleAction::TestRuleAction(const TestRuleAction&) = default;
TestRuleAction& TestRuleAction::operator=(const TestRuleAction&) = default;

base::Value::Dict TestRuleAction::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kRuleActionTypeKey, type);
  SetValue(dict, kRequestHeadersKey, request_headers);
  SetValue(dict, kResponseHeadersKey, response_headers);
  SetValue(dict, kRedirectKey, redirect);
  return dict;
}

TestRule::TestRule() = default;
TestRule::~TestRule() = default;
TestRule::TestRule(const TestRule&) = default;
TestRule& TestRule::operator=(const TestRule&) = default;

base::Value::Dict TestRule::ToValue() const {
  base::Value::Dict dict;
  SetValue(dict, kIDKey, id);
  SetValue(dict, kPriorityKey, priority);
  SetValue(dict, kRuleConditionKey, condition);
  SetValue(dict, kRuleActionKey, action);
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
                                 base::Value::List rules_value,
                                 bool enabled)
    : TestRulesetInfo(manifest_id_and_path,
                      manifest_id_and_path,
                      std::move(rules_value),
                      enabled) {}

TestRulesetInfo::TestRulesetInfo(const std::string& manifest_id,
                                 const std::string& relative_file_path,
                                 base::Value::List rules_value,
                                 bool enabled)
    : manifest_id(manifest_id),
      relative_file_path(relative_file_path),
      rules_value(std::move(rules_value)),
      enabled(enabled) {}

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
                      info.rules_value.Clone(),
                      info.enabled) {}

base::Value::Dict TestRulesetInfo::GetManifestValue() const {
  dnr_api::Ruleset ruleset;
  ruleset.id = manifest_id;
  ruleset.path = relative_file_path;
  ruleset.enabled = enabled;
  return ruleset.ToValue();
}

base::Value::Dict CreateManifest(
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

  base::Value::Dict manifest_builder;

  if (flags & kConfig_OmitDeclarativeNetRequestKey) {
    DCHECK(ruleset_info.empty());
  } else {
    manifest_builder.Set(
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        base::Value::Dict().Set(dnr_api::DNRInfo::kRuleResources,
                                ToValue(ruleset_info)));
  }

  if (flags & kConfig_HasManifestSandbox) {
    manifest_builder.SetByDottedPath(
        keys::kSandboxedPages,
        base::Value::List().Append(kManifestSandboxPageFilepath));
  }

  // std::move() to trigger rvalue overloads.
  return std::move(manifest_builder)
      .Set(keys::kName, extension_name)
      .Set(keys::kPermissions, ToValue(permissions))
      .Set(keys::kVersion, "1.0")
      .Set(keys::kManifestVersion, 2)
      .Set("background",
           base::Value::Dict().Set("scripts", ToValue(background_scripts)))
      .Set(keys::kBrowserAction, base::Value::Dict());
}

base::Value::List ToListValue(const std::vector<std::string>& vec) {
  return ToValue(vec);
}

base::Value::List ToListValue(const std::vector<TestRule>& rules) {
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

  // Persist a background script if needed.
  if (flags & ConfigFlag::kConfig_HasBackgroundScript) {
    static constexpr char kScriptWithOnUpdateAvailable[] =
        "chrome.runtime.onUpdateAvailable.addListener(() => {});"
        "chrome.test.sendMessage('ready');";

    std::string content = flags & ConfigFlag::kConfig_ListenForOnUpdateAvailable
                              ? kScriptWithOnUpdateAvailable
                              : "chrome.test.sendMessage('ready');";
    CHECK(base::WriteFile(extension_dir.Append(kBackgroundScriptFilepath),
                          content));
  }

  // Persist a manifest sandbox page if needed.
  if (flags & ConfigFlag::kConfig_HasManifestSandbox) {
    static constexpr char kManifestSandboxPage[] = "<html></html>";

    CHECK(
        base::WriteFile(extension_dir.AppendASCII(kManifestSandboxPageFilepath),
                        kManifestSandboxPage));
  }

  // Persist manifest file.
  JSONFileValueSerializer(extension_dir.Append(kManifestFilename))
      .Serialize(CreateManifest(ruleset_info, hosts, flags, extension_name));
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
