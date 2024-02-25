// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
#define EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__

#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// This is a trivial test RulesRegistry that can only store and retrieve rules.
class TestRulesRegistry : public RulesRegistry {
 public:
  TestRulesRegistry(const std::string& event_name, int rules_registry_id);
  TestRulesRegistry(content::BrowserContext* browser_context,
                    const std::string& event_name,
                    RulesCacheDelegate* cache_delegate,
                    int rules_registry_id);

  TestRulesRegistry(const TestRulesRegistry&) = delete;
  TestRulesRegistry& operator=(const TestRulesRegistry&) = delete;

  // RulesRegistry implementation:
  std::string AddRulesImpl(
      const ExtensionId& extension_id,
      const std::vector<const api::events::Rule*>& rules) override;
  std::string RemoveRulesImpl(
      const ExtensionId& extension_id,
      const std::vector<std::string>& rule_identifiers) override;
  std::string RemoveAllRulesImpl(const ExtensionId& extension_id) override;

  // Sets the result message that will be returned by the next call of
  // AddRulesImpl, RemoveRulesImpl and RemoveAllRulesImpl.
  void SetResult(const std::string& result);

 protected:
  ~TestRulesRegistry() override;

 private:
  // The string that gets returned by the implementation functions of
  // RulesRegistry. Defaults to "".
  std::string result_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_TEST_RULES_REGISTRY_H__
