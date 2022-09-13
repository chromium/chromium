// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_GLOBAL_RULES_TRACKER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_GLOBAL_RULES_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ExtensionPrefs;
class ExtensionRegistry;

namespace declarative_net_request {

class GlobalRulesTracker {
 public:
  explicit GlobalRulesTracker(ExtensionPrefs* extension_prefs,
                              ExtensionRegistry* extension_registry);
  ~GlobalRulesTracker();
  GlobalRulesTracker(const GlobalRulesTracker& other) = delete;
  GlobalRulesTracker& operator=(const GlobalRulesTracker& other) = delete;

  // Called when an extension's allocated static rule count is updated.
  // Returns whether the extension's new rule count will result in the total
  // rule count staying within the global rule limit. The extension's allocated
  // rule count is updated iff true is returned.
  bool OnExtensionRuleCountUpdated(const ExtensionId& extension_id,
                                   size_t new_rule_count);

  // Returns the number of unallocated rules in the global pool.
  size_t GetUnallocatedRuleCount() const;

  // Returns the number of rules in the global pool available for the extension
  // before the global limit is reached. This includes the extension's global
  // pool allocation.
  size_t GetAvailableAllocation(const ExtensionId& extension_id) const;

  // Clears the extension's allocated rule count.
  void ClearExtensionAllocation(const ExtensionId& extension_id);

  // Returns |allocated_global_rule_count_|.
  size_t GetAllocatedGlobalRuleCountForTesting() const;

 private:
  // Returns the number of rules in the global pool allocated for the extension.
  // Note: Some of these allocated rules might not be enabled by the extension.
  size_t GetAllocationInPrefs(const ExtensionId& extension_id) const;

  // The number of static rules from all extensions which contribute to the
  // global rule pool. Any enabled static rules for an extension past
  // |GUARANTEED_MINIMUM_STATIC_RULES| count towards this. This value must never
  // exceed |kMaxStaticRulesPerProfile|.
  size_t allocated_global_rule_count_ = 0;

  const raw_ptr<ExtensionPrefs> extension_prefs_;
  const raw_ptr<ExtensionRegistry> extension_registry_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_GLOBAL_RULES_TRACKER_H_
