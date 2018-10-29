// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/test_utils.h"

#include <string>

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"

namespace extensions {
namespace declarative_net_request {

bool HasValidIndexedRuleset(const Extension& extension,
                            content::BrowserContext* browser_context) {
  int expected_checksum;
  if (!ExtensionPrefs::Get(browser_context)
           ->GetDNRRulesetChecksum(extension.id(), &expected_checksum)) {
    return false;
  }

  std::unique_ptr<RulesetMatcher> matcher;
  return RulesetMatcher::CreateVerifiedMatcher(
             file_util::GetIndexedRulesetPath(extension.path()),
             expected_checksum, &matcher) == RulesetMatcher::kLoadSuccess;
}

}  // namespace declarative_net_request
}  // namespace extensions
