// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/global_rules_tracker.h"

#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension.h"

namespace extensions::declarative_net_request {

namespace {

namespace dnr_api = api::declarative_net_request;

// Returns the total allocated global rule count, as maintained in extension
// prefs from the set of installed extensions in the registry.
size_t CalculateAllocatedGlobalRuleCount(
    ExtensionPrefs* extension_prefs,
    const ExtensionRegistry* extension_registry) {
  const ExtensionSet installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();
  const PrefsHelper helper(*extension_prefs);

  // For each extension, fetch its allocated rules count and add it to
  // |allocated_global_rule_count_|.
  size_t allocated_global_rule_count = 0;
  for (const auto& extension : installed_extensions) {
    int allocated_rule_count = 0;
    if (helper.GetAllocatedGlobalRuleCount(extension->id(),
                                           allocated_rule_count)) {
      allocated_global_rule_count += allocated_rule_count;
    }
  }

  return allocated_global_rule_count;
}

// Returns the total allocated global rule count, as maintained in extension
// prefs from the set of installed extensions from prefs. This should be called
// only if the extension registry has not been populated yet (e.g. when a
// browser session has just started).
size_t CalculateInitialAllocatedGlobalRuleCount(
    ExtensionPrefs* extension_prefs) {
  const ExtensionPrefs::ExtensionsInfo extensions_info =
      extension_prefs->GetInstalledExtensionsInfo();
  const PrefsHelper helper(*extension_prefs);

  // For each extension, fetch its allocated rules count and add it to
  // |allocated_global_rule_count_|.
  size_t allocated_global_rule_count = 0;
  for (const auto& info : extensions_info) {
    // Skip extensions that were loaded from the command-line because we don't
    // want those to persist across browser restart.
    if (info.extension_location == mojom::ManifestLocation::kCommandLine) {
      continue;
    }

    int allocated_rule_count = 0;
    if (helper.GetAllocatedGlobalRuleCount(info.extension_id,
                                           allocated_rule_count)) {
      allocated_global_rule_count += allocated_rule_count;
    }
  }

  return allocated_global_rule_count;
}

}  // namespace

GlobalRulesTracker::GlobalRulesTracker(ExtensionPrefs* extension_prefs,
                                       ExtensionRegistry* extension_registry)
    : allocated_global_rule_count_(
          CalculateInitialAllocatedGlobalRuleCount(extension_prefs)),
      extension_prefs_(extension_prefs),
      extension_registry_(extension_registry) {}

GlobalRulesTracker::~GlobalRulesTracker() = default;

size_t GlobalRulesTracker::GetAllocatedGlobalRuleCountForTesting() const {
  DCHECK_EQ(
      allocated_global_rule_count_,
      CalculateAllocatedGlobalRuleCount(extension_prefs_, extension_registry_));
  return allocated_global_rule_count_;
}

bool GlobalRulesTracker::OnExtensionRuleCountUpdated(
    const ExtensionId& extension_id,
    size_t new_rule_count) {
  // Each extension ruleset is allowed to have up to
  // |GetMaximumRulesPerRuleset()| rules during indexing.
  DCHECK_LE(new_rule_count, static_cast<size_t>(GetMaximumRulesPerRuleset()) *
                                dnr_api::MAX_NUMBER_OF_STATIC_RULESETS);

  PrefsHelper helper(*extension_prefs_);
  bool keep_excess_allocation = helper.GetKeepExcessAllocation(extension_id);

  if (new_rule_count <=
      static_cast<size_t>(GetStaticGuaranteedMinimumRuleCount())) {
    if (!keep_excess_allocation) {
      ClearExtensionAllocation(extension_id);
    }

    return true;
  }

  size_t old_allocated_rule_count = GetAllocationInPrefs(extension_id);
  DCHECK_GE(allocated_global_rule_count_, old_allocated_rule_count);

  size_t new_allocated_rule_count =
      new_rule_count - GetStaticGuaranteedMinimumRuleCount();

  if (new_allocated_rule_count == old_allocated_rule_count) {
    return true;
  }

  if (keep_excess_allocation &&
      old_allocated_rule_count > new_allocated_rule_count) {
    // Retain the extension's current excess allocation and allow the update.
    return true;
  }

  size_t new_global_rule_count =
      (allocated_global_rule_count_ - old_allocated_rule_count) +
      new_allocated_rule_count;

  // If updating this extension's rule count would cause the global rule count
  // to be exceeded, don't commit the update and return false.
  if (new_global_rule_count > static_cast<size_t>(GetGlobalStaticRuleLimit())) {
    return false;
  }

  if (keep_excess_allocation) {
    DCHECK_GT(new_allocated_rule_count, old_allocated_rule_count);
    // The extension is now using more than its pre-update rule allocation.
    // Remove its ability to keep the excess allocation.
    helper.SetKeepExcessAllocation(extension_id, false);
  }

  allocated_global_rule_count_ = new_global_rule_count;
  helper.SetAllocatedGlobalRuleCount(extension_id, new_allocated_rule_count);

  return true;
}

size_t GlobalRulesTracker::GetUnallocatedRuleCount() const {
  return GetGlobalStaticRuleLimit() - allocated_global_rule_count_;
}

size_t GlobalRulesTracker::GetAvailableAllocation(
    const ExtensionId& extension_id) const {
  return GetUnallocatedRuleCount() + GetAllocationInPrefs(extension_id);
}

void GlobalRulesTracker::ClearExtensionAllocation(
    const ExtensionId& extension_id) {
  int allocated_rule_count = 0;
  PrefsHelper helper(*extension_prefs_);
  if (!helper.GetAllocatedGlobalRuleCount(extension_id, allocated_rule_count)) {
    return;
  }

  DCHECK_GE(allocated_global_rule_count_,
            static_cast<size_t>(allocated_rule_count));

  allocated_global_rule_count_ -= allocated_rule_count;
  helper.SetAllocatedGlobalRuleCount(extension_id,
                                     /*rule_count=*/0);
}

size_t GlobalRulesTracker::GetAllocationInPrefs(
    const ExtensionId& extension_id) const {
  const PrefsHelper helper(*extension_prefs_);
  int allocated_rule_count = 0;
  helper.GetAllocatedGlobalRuleCount(extension_id, allocated_rule_count);
  return allocated_rule_count;
}

}  // namespace extensions::declarative_net_request
