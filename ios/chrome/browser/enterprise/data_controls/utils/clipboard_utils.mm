// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/clipboard_utils.h"

#import <utility>

#import "base/check.h"
#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "url/gurl.h"

namespace data_controls {

PastePolicyVerdict IsPasteAllowedByPolicy(
    const GURL& source_url,
    const GURL& destination_url,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile) {
  CHECK(destination_profile);
  // TODO(crbug.com/438202190): This is the place holder for paste policy
  // evaluation API.

  PastePolicyVerdict policy_verdict{Verdict::NotSet(), false};

  IOSRulesService* rules_service =
      IOSRulesServiceFactory::GetForProfile(destination_profile);
  CHECK(rules_service);

  policy_verdict.verdict = rules_service->GetPasteVerdict(
      source_url, destination_url, source_profile, destination_profile);
  return policy_verdict;
}

CopyPolicyVerdicts IsCopyAllowedByPolicy(const GURL& source_url,
                                         const ui::ClipboardMetadata& metadata,
                                         ProfileIOS* source_profile) {
  CHECK(source_profile);

  IOSRulesService* rules_service =
      IOSRulesServiceFactory::GetForProfile(source_profile);
  auto verdict = rules_service->GetCopyRestrictedBySourceVerdict(source_url);

  if (verdict.level() == Rule::Level::kBlock) {
    return {std::move(verdict), false};
  }

  auto os_clipboard_verdict =
      rules_service->GetCopyToOSClipboardVerdict(source_url);
  bool allow_copy_to_os = os_clipboard_verdict.level() != Rule::Level::kBlock;

  if (verdict.level() == Rule::Level::kWarn ||
      os_clipboard_verdict.level() == Rule::Level::kWarn) {
    verdict = Verdict::MergeCopyWarningVerdicts(
        std::move(verdict), std::move(os_clipboard_verdict));
  }

  return {std::move(verdict), allow_copy_to_os};
}

}  // namespace data_controls
