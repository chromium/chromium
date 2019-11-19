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
#include "extensions/common/url_pattern.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
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

  std::unique_ptr<base::DictionaryValue> ToValue() const override;
};

struct TestRuleAction : public DictionarySource {
  TestRuleAction();
  ~TestRuleAction() override;
  TestRuleAction(const TestRuleAction&);
  TestRuleAction& operator=(const TestRuleAction&);

  base::Optional<std::string> type;
  base::Optional<std::vector<std::string>> remove_headers_list;
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
TestRule CreateGenericRule();

// Helper to build an extension manifest which uses the
// kDeclarativeNetRequestKey manifest key. |hosts| specifies the host
// permissions to grant. If |has_background_script| is true, the manifest
// returned will have "background.js" as its background script.
std::unique_ptr<base::DictionaryValue> CreateManifest(
    const std::string& json_rules_filename,
    const std::vector<std::string>& hosts = {},
    bool has_background_script = false);

// Returns a ListValue corresponding to a vector of strings.
std::unique_ptr<base::ListValue> ToListValue(
    const std::vector<std::string>& vec);

// Writes the declarative |rules| in the given |extension_dir| together with the
// manifest file. |hosts| specifies the host permissions, the extensions should
// have. If |has_background_script| is true, a background script
// ("background.js") will also be persisted for the extension. Clients can
// listen in to the "ready" message from the background page to detect its
// loading.
void WriteManifestAndRuleset(
    const base::FilePath& extension_dir,
    const base::FilePath::CharType* json_rules_filepath,
    const std::string& json_rules_filename,
    const std::vector<TestRule>& rules,
    const std::vector<std::string>& hosts,
    bool has_background_script = false);
void WriteManifestAndRuleset(
    const base::FilePath& extension_dir,
    const base::FilePath::CharType* json_rules_filepath,
    const std::string& json_rules_filename,
    const base::Value& rules,
    const std::vector<std::string>& hosts,
    bool has_background_script = false);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
