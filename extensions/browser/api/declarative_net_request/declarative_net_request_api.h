// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_

#include <string>

#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/http/http_response_headers.h"

namespace extensions {

namespace declarative_net_request {
class CompositeMatcher;
struct ReadJSONRulesResult;
struct RequestAction;
struct RequestParams;
}  // namespace declarative_net_request

namespace api::declarative_net_request::GetDynamicRules {
struct Params;
}

class DeclarativeNetRequestUpdateDynamicRulesFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestUpdateDynamicRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.updateDynamicRules",
                             DECLARATIVENETREQUEST_UPDATEDYNAMICRULES)

 protected:
  ~DeclarativeNetRequestUpdateDynamicRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnDynamicRulesUpdated(std::optional<std::string> error);
};

class DeclarativeNetRequestGetDynamicRulesFunction : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetDynamicRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getDynamicRules",
                             DECLARATIVENETREQUEST_GETDYNAMICRULES)

 protected:
  ~DeclarativeNetRequestGetDynamicRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnDynamicRulesFetched(
      api::declarative_net_request::GetDynamicRules::Params params,
      declarative_net_request::ReadJSONRulesResult read_json_result);
};

class DeclarativeNetRequestUpdateSessionRulesFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestUpdateSessionRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.updateSessionRules",
                             DECLARATIVENETREQUEST_UPDATESESSIONRULES)

 protected:
  ~DeclarativeNetRequestUpdateSessionRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnSessionRulesUpdated(std::optional<std::string> error);
};

class DeclarativeNetRequestGetSessionRulesFunction : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetSessionRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getSessionRules",
                             DECLARATIVENETREQUEST_GETSESSIONRULES)

 protected:
  ~DeclarativeNetRequestGetSessionRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestUpdateEnabledRulesetsFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestUpdateEnabledRulesetsFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.updateEnabledRulesets",
                             DECLARATIVENETREQUEST_UPDATEENABLEDRULESETS)

 protected:
  ~DeclarativeNetRequestUpdateEnabledRulesetsFunction() override;

 private:
  void OnEnabledStaticRulesetsUpdated(std::optional<std::string> error);

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestGetEnabledRulesetsFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetEnabledRulesetsFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getEnabledRulesets",
                             DECLARATIVENETREQUEST_GETENABLEDRULESETS)

 protected:
  ~DeclarativeNetRequestGetEnabledRulesetsFunction() override;

 private:
  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestUpdateStaticRulesFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestUpdateStaticRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.updateStaticRules",
                             DECLARATIVENETREQUEST_UPDATESTATICRULES)

 protected:
  ~DeclarativeNetRequestUpdateStaticRulesFunction() override;

 private:
  void OnStaticRulesUpdated(std::optional<std::string> error);

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestGetDisabledRuleIdsFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetDisabledRuleIdsFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getDisabledRuleIds",
                             DECLARATIVENETREQUEST_GETDISABLEDRULEIDS)

 protected:
  ~DeclarativeNetRequestGetDisabledRuleIdsFunction() override;

 private:
  void OnDisabledRuleIdsRead(std::vector<int> disabled_rule_ids);

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestGetMatchedRulesFunction : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetMatchedRulesFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.getMatchedRules",
                             DECLARATIVENETREQUEST_GETMATCHEDRULES)

  static void set_disable_throttling_for_tests(
      bool disable_throttling_for_test) {
    disable_throttling_for_test_ = disable_throttling_for_test;
  }

 protected:
  ~DeclarativeNetRequestGetMatchedRulesFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
  void GetQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) const override;
  bool ShouldSkipQuotaLimiting() const override;

 private:
  static bool disable_throttling_for_test_;
};

class DeclarativeNetRequestSetExtensionActionOptionsFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestSetExtensionActionOptionsFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.setExtensionActionOptions",
                             DECLARATIVENETREQUEST_SETACTIONCOUNTASBADGETEXT)

 protected:
  ~DeclarativeNetRequestSetExtensionActionOptionsFunction() override;

  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestIsRegexSupportedFunction : public ExtensionFunction {
 public:
  DeclarativeNetRequestIsRegexSupportedFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.isRegexSupported",
                             DECLARATIVENETREQUEST_ISREGEXSUPPORTED)

 protected:
  ~DeclarativeNetRequestIsRegexSupportedFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestGetAvailableStaticRuleCountFunction
    : public ExtensionFunction {
 public:
  DeclarativeNetRequestGetAvailableStaticRuleCountFunction();
  DECLARE_EXTENSION_FUNCTION(
      "declarativeNetRequest.getAvailableStaticRuleCount",
      DECLARATIVENETREQUEST_GETAVAILABLESTATICRULECOUNT)

 protected:
  ~DeclarativeNetRequestGetAvailableStaticRuleCountFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;
};

class DeclarativeNetRequestTestMatchOutcomeFunction : public ExtensionFunction {
 public:
  DeclarativeNetRequestTestMatchOutcomeFunction();
  DECLARE_EXTENSION_FUNCTION("declarativeNetRequest.testMatchOutcome",
                             DECLARATIVENETREQUEST_TESTMATCHOUTCOME)

 protected:
  ~DeclarativeNetRequestTestMatchOutcomeFunction() override;

  // ExtensionFunction override:
  ExtensionFunction::ResponseAction Run() override;

 private:
  using TestResponseHeaders =
      api::declarative_net_request::TestMatchRequestDetails::ResponseHeaders;

  // Parse `test_headers` provided by the API function's args into a headers
  // object. Populates `error` and returns null if parsing fails or if the
  // resultant headers contain invalid names or values.
  scoped_refptr<const net::HttpResponseHeaders> ParseHeaders(
      std::optional<TestResponseHeaders>& test_headers,
      std::string& error) const;

  // Creates a base::Value::List which wraps a list of dnr_api::MatchedRule from
  // the provided `actions`. The base::Value::List will be returned as part of
  // this API function's response.
  base::Value::List CreateMatchedRulesFromActions(
      const std::vector<declarative_net_request::RequestAction>& actions) const;

  // Returns a list of matching actions for the given request `params` against
  // this extension's `matcher`.
  std::vector<declarative_net_request::RequestAction> GetActions(
      const declarative_net_request::CompositeMatcher& matcher,
      const declarative_net_request::RequestParams& params,
      declarative_net_request::RulesetMatchingStage stage,
      PermissionsData::PageAccess page_access) const;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_DECLARATIVE_NET_REQUEST_API_H_
