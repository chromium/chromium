// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_state_tester.h"
#include "base/memory/raw_ref.h"

#include "base/strings/stringprintf.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kEnabledSet[] = "enabled";
constexpr char kDisabledSet[] = "disabled";
constexpr char kTerminatedSet[] = "terminated";
constexpr char kBlocklistedSet[] = "blocklisted";

}  // namespace

ExtensionStateTester::ExtensionStateTester(
    content::BrowserContext* browser_context)
    : registry_(ExtensionRegistry::Get(browser_context)),
      prefs_(ExtensionPrefs::Get(browser_context)) {}

ExtensionStateTester::~ExtensionStateTester() = default;

bool ExtensionStateTester::ExpectEnabled(const ExtensionId& extension_id) {
  bool success = ExpectOnlyInSet(extension_id, kEnabledSet);
  int disable_reasons = prefs_->GetDisableReasons(extension_id);
  if (disable_reasons != 0) {
    success = false;
    ADD_FAILURE() << "Extension '" << extension_id
                  << "' had unexpected disable reasons: " << disable_reasons;
  }

  return success;
}

bool ExtensionStateTester::ExpectDisabledWithSingleReason(
    const ExtensionId& extension_id,
    disable_reason::DisableReason expected_reason) {
  bool success = ExpectOnlyInSet(extension_id, kDisabledSet);
  int disable_reasons = prefs_->GetDisableReasons(extension_id);

  // NOTE(devlin): We could make this more helpful in error logging by having
  // a mapping of string -> disable reasons, and comparing the vector.
  if (disable_reasons == 0) {
    success = false;
    ADD_FAILURE() << "Extension should have disable reason " << expected_reason
                  << ", but has no disable reasons.";
  } else if ((disable_reasons & expected_reason) == 0) {
    success = false;
    ADD_FAILURE() << "Extension should have disable reason " << expected_reason
                  << ", but instead has disable reasons " << disable_reasons;
  } else if (disable_reasons != expected_reason) {
    success = false;
    ADD_FAILURE() << "Extension has additional unexpected disable reasons. "
                  << "Expected only " << expected_reason << ", but found "
                  << disable_reasons;
  }
  // Else, disable reasons are as expected.

  return success;
}

bool ExtensionStateTester::ExpectDisabledWithReasons(
    const ExtensionId& extension_id,
    int expected_reasons) {
  bool success = ExpectOnlyInSet(extension_id, kDisabledSet);
  int disable_reasons = prefs_->GetDisableReasons(extension_id);

  if (disable_reasons == 0) {
    success = false;
    ADD_FAILURE() << "Extension should have disable reasons "
                  << expected_reasons << ", but has no disable reasons.";
  } else if (disable_reasons != expected_reasons) {
    success = false;
    ADD_FAILURE() << "Extension has different disable reasons than expected. "
                  << "Expected " << expected_reasons << ", but found "
                  << disable_reasons;
  }
  // Else, disable reasons are as expected.

  return success;
}

bool ExtensionStateTester::ExpectBlocklisted(const ExtensionId& extension_id) {
  return ExpectOnlyInSet(extension_id, kBlocklistedSet);
}

bool ExtensionStateTester::ExpectTerminated(const ExtensionId& extension_id) {
  return ExpectOnlyInSet(extension_id, kTerminatedSet);
}

bool ExtensionStateTester::ExpectOnlyInSet(const ExtensionId& extension_id,
                                           const char* expected_set_name) {
  struct {
    const raw_ref<const ExtensionSet> extensions;
    const char* set_name;
  } registry_sets[] = {
      {ToRawRef(registry_->enabled_extensions()), kEnabledSet},
      {ToRawRef(registry_->disabled_extensions()), kDisabledSet},
      {ToRawRef(registry_->terminated_extensions()), kTerminatedSet},
      {ToRawRef(registry_->blocklisted_extensions()), kBlocklistedSet},
  };

  auto get_error = [extension_id](const char* set_name, bool expected_in_set) {
    if (expected_in_set) {
      return base::StringPrintf(
          "Extension with id '%s' was expected in the '%s' registry set,"
          " but was not present",
          extension_id.c_str(), set_name);
    }
    return base::StringPrintf(
        "Extension with id '%s' was unexpectedly found in the '%s' "
        "registry set.",
        extension_id.c_str(), set_name);
  };

  bool succeeded = true;
  for (const auto& set : registry_sets) {
    bool expected_in_set = set.set_name == expected_set_name;
    bool is_in_set = set.extensions->Contains(extension_id);
    if (expected_in_set == is_in_set)
      continue;  // Extension is in the set we expect it.

    succeeded = false;
    ADD_FAILURE() << get_error(set.set_name, expected_in_set);
  }

  return succeeded;
}

}  // namespace extensions
