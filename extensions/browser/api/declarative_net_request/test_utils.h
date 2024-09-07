// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace extensions {

class Extension;

namespace declarative_net_request {

class FileBackedRulesetSource;
struct RequestParams;
class RulesetMatcher;
struct RuleCounts;
struct TestRule;

// Enum specifying the extension load type. Used for parameterized tests.
enum class ExtensionLoadType {
  PACKED,
  UNPACKED,
};

// Factory method to construct a RequestAction given a RequestAction type and
// optionally, an ExtensionId.
RequestAction CreateRequestActionForTesting(
    RequestAction::Type type,
    uint32_t rule_id = kMinValidID,
    uint32_t rule_priority = kDefaultPriority,
    RulesetID ruleset_id = kMinValidStaticRulesetID,
    const ExtensionId& extension_id = "extensionid");

// Test helpers for help with gtest expectations and assertions.
bool operator==(const RequestAction& lhs, const RequestAction& rhs);
std::ostream& operator<<(std::ostream& output, RequestAction::Type type);
std::ostream& operator<<(std::ostream& output, const RequestAction& action);
std::ostream& operator<<(std::ostream& output, const ParseResult& result);
std::ostream& operator<<(std::ostream& output,
                         const std::optional<RequestAction>& action);
std::ostream& operator<<(std::ostream& output, LoadRulesetResult result);
std::ostream& operator<<(std::ostream& output, const RuleCounts& count);

// Returns true if the given extension's indexed static rulesets are all valid.
// Should be called on a sequence where file IO is allowed.
bool AreAllIndexedStaticRulesetsValid(
    const Extension& extension,
    content::BrowserContext* browser_context,
    FileBackedRulesetSource::RulesetFilter ruleset_filter);

// Helper to create a verified ruleset matcher. Populates |matcher| and
// |expected_checksum|. Returns true on success.
bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const FileBackedRulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum = nullptr);

// Helper to return a FileBackedRulesetSource bound to temporary files.
FileBackedRulesetSource CreateTemporarySource(
    RulesetID id = kMinValidStaticRulesetID,
    size_t rule_count_limit = 100,
    ExtensionId extension_id = "extensionid");

api::declarative_net_request::ModifyHeaderInfo CreateModifyHeaderInfo(
    api::declarative_net_request::HeaderOperation operation,
    std::string header,
    std::optional<std::string> value,
    std::optional<std::string> regex_filter = std::nullopt,
    std::optional<std::string> regex_substitution = std::nullopt,
    std::optional<api::declarative_net_request::HeaderRegexOptions>
        regex_options = std::nullopt);

bool EqualsForTesting(
    const api::declarative_net_request::ModifyHeaderInfo& lhs,
    const api::declarative_net_request::ModifyHeaderInfo& rhs);

api::declarative_net_request::HeaderInfo CreateHeaderInfo(
    std::string header,
    std::optional<std::vector<std::string>> values,
    std::optional<std::vector<std::string>> excluded_values);

// Test observer for RulesetManager. This is a multi-use observer i.e.
// WaitForExtensionsWithRulesetsCount can be called multiple times per lifetime
// of an observer.
class RulesetManagerObserver : public RulesetManager::TestObserver {
 public:
  explicit RulesetManagerObserver(RulesetManager* manager);
  RulesetManagerObserver(const RulesetManagerObserver&) = delete;
  RulesetManagerObserver& operator=(const RulesetManagerObserver&) = delete;
  ~RulesetManagerObserver() override;

  // Returns the requests seen by RulesetManager since the last call to this
  // function.
  std::vector<GURL> GetAndResetRequestSeen();

  // Waits for the number of rulesets to change to |count|. Note |count| is the
  // number of extensions with rulesets or the number of active
  // CompositeMatchers.
  void WaitForExtensionsWithRulesetsCount(size_t count);

 private:
  // RulesetManager::TestObserver implementation.
  void OnRulesetCountChanged(size_t count) override;
  void OnEvaluateRequest(const WebRequestInfo& request,
                         bool is_incognito_context) override;

  const raw_ptr<RulesetManager> manager_;
  size_t current_count_ = 0;
  std::optional<size_t> expected_count_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::vector<GURL> observed_requests_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Helper to wait for warnings thrown for a given extension. This must be
// constructed before warnings are added.
class WarningServiceObserver : public WarningService::Observer {
 public:
  WarningServiceObserver(WarningService* warning_service,
                         const ExtensionId& extension_id);
  ~WarningServiceObserver();
  WarningServiceObserver(const WarningServiceObserver&) = delete;
  WarningServiceObserver& operator=(const WarningServiceObserver&) = delete;

  // Should only be called once per WarningServiceObserver lifetime.
  void WaitForWarning();

 private:
  // WarningService::Observer override:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override;

  base::ScopedObservation<WarningService, WarningService::Observer>
      observation_{this};
  const ExtensionId extension_id_;
  base::RunLoop run_loop_;
};

base::flat_set<int> GetDisabledRuleIdsFromMatcherForTesting(
    const RulesetManager& ruleset_manager,
    const Extension& extension,
    const std::string& ruleset_id_string);

RequestParams CreateRequestWithResponseHeaders(
    const GURL& url,
    const net::HttpResponseHeaders* headers);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
