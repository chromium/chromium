// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/url_pattern.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {
namespace declarative_net_request {

struct DictionarySource {
  DictionarySource() = default;
  virtual ~DictionarySource() = default;
  virtual std::unique_ptr<base::DictionaryValue> ToValue() const = 0;
};

// Helper structs to simplify building base::Values which can later be used to
// serialize to JSON. The generated implementation for the JSON rules schema is
// not used since it's not flexible enough to generate the base::Value/JSON we
// want for tests.
struct TestRuleCondition : public DictionarySource {
  TestRuleCondition();
  ~TestRuleCondition() override;
  TestRuleCondition(const TestRuleCondition&);
  TestRuleCondition& operator=(const TestRuleCondition&);

  base::Optional<std::string> url_filter;
  base::Optional<std::string> regex_filter;
  base::Optional<bool> is_url_filter_case_sensitive;
  base::Optional<std::vector<std::string>> domains;
  base::Optional<std::vector<std::string>> excluded_domains;
  base::Optional<std::vector<std::string>> resource_types;
  base::Optional<std::vector<std::string>> excluded_resource_types;
  base::Optional<std::string> domain_type;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleQueryKeyValue : public DictionarySource {
  TestRuleQueryKeyValue();
  ~TestRuleQueryKeyValue() override;
  TestRuleQueryKeyValue(const TestRuleQueryKeyValue&);
  TestRuleQueryKeyValue& operator=(const TestRuleQueryKeyValue&);

  base::Optional<std::string> key;
  base::Optional<std::string> value;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleQueryTransform : public DictionarySource {
  TestRuleQueryTransform();
  ~TestRuleQueryTransform() override;
  TestRuleQueryTransform(const TestRuleQueryTransform&);
  TestRuleQueryTransform& operator=(const TestRuleQueryTransform&);

  base::Optional<std::vector<std::string>> remove_params;
  base::Optional<std::vector<TestRuleQueryKeyValue>> add_or_replace_params;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleTransform : public DictionarySource {
  TestRuleTransform();
  ~TestRuleTransform() override;
  TestRuleTransform(const TestRuleTransform&);
  TestRuleTransform& operator=(const TestRuleTransform&);

  base::Optional<std::string> scheme;
  base::Optional<std::string> host;
  base::Optional<std::string> port;
  base::Optional<std::string> path;
  base::Optional<std::string> query;
  base::Optional<TestRuleQueryTransform> query_transform;
  base::Optional<std::string> fragment;
  base::Optional<std::string> username;
  base::Optional<std::string> password;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleRedirect : public DictionarySource {
  TestRuleRedirect();
  ~TestRuleRedirect() override;
  TestRuleRedirect(const TestRuleRedirect&);
  TestRuleRedirect& operator=(const TestRuleRedirect&);

  base::Optional<std::string> extension_path;
  base::Optional<TestRuleTransform> transform;
  base::Optional<std::string> url;
  base::Optional<std::string> regex_substitution;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestHeaderInfo : public DictionarySource {
  TestHeaderInfo(std::string header,
                 std::string operation,
                 base::Optional<std::string> value);
  ~TestHeaderInfo() override;
  TestHeaderInfo(const TestHeaderInfo&);
  TestHeaderInfo& operator=(const TestHeaderInfo&);

  base::Optional<std::string> header;
  base::Optional<std::string> operation;
  base::Optional<std::string> value;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleAction : public DictionarySource {
  TestRuleAction();
  ~TestRuleAction() override;
  TestRuleAction(const TestRuleAction&);
  TestRuleAction& operator=(const TestRuleAction&);

  base::Optional<std::string> type;
  base::Optional<std::vector<TestHeaderInfo>> request_headers;
  base::Optional<std::vector<TestHeaderInfo>> response_headers;
  base::Optional<TestRuleRedirect> redirect;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRule : public DictionarySource {
  TestRule();
  ~TestRule() override;
  TestRule(const TestRule&);
  TestRule& operator=(const TestRule&);

  base::Optional<int> id;
  base::Optional<int> priority;
  base::Optional<TestRuleCondition> condition;
  base::Optional<TestRuleAction> action;

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

// Helper function to build a generic TestRule.
TestRule CreateGenericRule(int id = kMinValidID);

// Helper function to build a generic regex TestRule.
TestRule CreateRegexRule(int id = kMinValidID);

// Bitmasks to configure the extension under test.
enum ConfigFlag {
  kConfig_None = 0,

  // Whether a background script ("background.js") will be persisted for the
  // extension. Clients can listen in to the "ready" message from the background
  // page to detect its loading.
  kConfig_HasBackgroundScript = 1 << 0,

  // Whether the extension has the declarativeNetRequestFeedback permission.
  kConfig_HasFeedbackPermission = 1 << 1,

  // Whether the extension has the activeTab permission.
  kConfig_HasActiveTab = 1 << 2,

  // Whether the "declarative_net_request" manifest key should be omitted.
  kConfig_OmitDeclarativeNetRequestKey = 1 << 3,

  // Whether the "declarativeNetRequest" permission should be omitted.
  kConfig_OmitDeclarativeNetRequestPermission = 1 << 4,
};

// Describes a single extension ruleset.
struct TestRulesetInfo {
  TestRulesetInfo(const std::string& manifest_id_and_path,
                  const base::Value& rules_value,
                  bool enabled = true);
  TestRulesetInfo(const std::string& manifest_id,
                  const std::string& relative_file_path,
                  const base::Value& rules_value,
                  bool enabled = true);
  TestRulesetInfo(const TestRulesetInfo&);
  TestRulesetInfo& operator=(const TestRulesetInfo&);

  // Unique ID for the ruleset.
  const std::string manifest_id;

  // File path relative to the extension directory.
  const std::string relative_file_path;

  // The base::Value corresponding to the rules in the ruleset.
  const base::Value rules_value;

  // Whether the ruleset is enabled by default.
  const bool enabled;

  // Returns the corresponding value to be specified in the manifest for the
  // ruleset.
  std::unique_ptr<base::DictionaryValue> GetManifestValue() const;
};

// Helper to build an extension manifest which uses the
// kDeclarativeNetRequestKey manifest key. |hosts| specifies the host
// permissions to grant. |flags| is a bitmask of ConfigFlag to configure the
// extension. |ruleset_info| specifies the static rulesets for the extension.
std::unique_ptr<base::DictionaryValue> CreateManifest(
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts = {},
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

// Returns a ListValue corresponding to a vector of strings.
std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& vec);

// Returns a ListValue corresponding to a vector of TestRules.
std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<TestRule>& rules);

// Writes the rulesets specified in |ruleset_info| in the given |extension_dir|
// together with the manifest file. |hosts| specifies the host permissions, the
// extensions should have. |flags| is a bitmask of ConfigFlag to configure the
// extension.
void WriteManifestAndRulesets(
    const base::FilePath& extension_dir,
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

// Specialization of WriteManifestAndRulesets above for an extension with a
// single static ruleset.
void WriteManifestAndRuleset(
    const base::FilePath& extension_dir,
    const TestRulesetInfo& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
