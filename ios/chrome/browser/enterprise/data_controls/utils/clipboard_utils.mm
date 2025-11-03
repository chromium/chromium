// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/utils/clipboard_utils.h"

#import <utility>

#import "base/check.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_reporting_event_router_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/ios_clipboard_context.h"
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

  auto des_verdict = IOSRulesServiceFactory::GetForProfile(destination_profile)
                         ->GetPasteVerdict(source_url, destination_url,
                                           source_profile, destination_profile);
  bool triggered_by_source = false;
  if (source_profile && source_profile != destination_profile) {
    auto source_verdict =
        IOSRulesServiceFactory::GetForProfile(source_profile)
            ->GetPasteVerdict(source_url, destination_url, source_profile,
                              destination_profile);
    if (des_verdict.level() < source_verdict.level()) {
      triggered_by_source = true;
    }
    des_verdict = Verdict::MergePasteVerdicts(std::move(source_verdict),
                                              std::move(des_verdict));
  }

  return {std::move(des_verdict), triggered_by_source};
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

Verdict IsShareAllowedByPolicy(const GURL& source_url,
                               ProfileIOS* source_profile) {
  CHECK(source_profile);

  IOSRulesService* rules_service =
      IOSRulesServiceFactory::GetForProfile(source_profile);
  // Once the default sharing sheet is presented, the user will be able to copy
  // the selected content to the os clipboard. Treat the share intent as a copy
  // to os clipboard intent.
  return rules_service->GetCopyToOSClipboardVerdict(source_url);
}

void MaybeReportDataControlsPaste(const GURL& source_url,
                                  const GURL& destination_url,
                                  ProfileIOS* source_profile,
                                  ProfileIOS* destination_profile,
                                  const ui::ClipboardMetadata& metadata,
                                  const Verdict& verdict,
                                  bool bypassed) {
  auto* router =
      enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
          destination_profile);
  if (!router) {
    return;
  }

  IOSClipboardContext context(source_url, destination_url, source_profile,
                              destination_profile, metadata);
  if (bypassed) {
    router->ReportPasteWarningBypassed(context, verdict);
  } else {
    router->ReportPaste(context, verdict);
  }
}

void MaybeReportDataControlsCopy(const GURL& source_url,
                                 ProfileIOS* source_profile,
                                 const ui::ClipboardMetadata& metadata,
                                 const Verdict& verdict,
                                 bool bypassed) {
  auto* router =
      enterprise_connectors::IOSReportingEventRouterFactory::GetForProfile(
          source_profile);
  if (!router) {
    return;
  }

  IOSClipboardContext context(source_url, GURL(), source_profile, nullptr,
                              metadata);
  if (bypassed) {
    router->ReportCopyWarningBypassed(context, verdict);
  } else {
    router->ReportCopy(context, verdict);
  }
}

}  // namespace data_controls
