// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/clipboard_utils.h"

#import "base/check.h"
#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "url/gurl.h"

namespace data_controls {

void IsPasteAllowedByPolicy(
    const ActionContext& action_context,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile,
    web::WebState* webState,
    IsClipboardPasteAllowedCallbackIOS callback) {
  // TODO(crbug.com/438202190): This is the place holder for paste policy
  // evaluation API.
}

void IsCopyAllowedByPolicy(const GURL& source_url,
                           const ui::ClipboardMetadata& metadata,
                           ProfileIOS* source_profile,
                           web::WebState* webState,
                           IsClipboardCopyAllowedCallbackIOS callback) {
  // TODO(crbug.com/438200537): This is the place holder for copy policy
  // evaluation API.
  CHECK(source_profile);

  IOSRulesService* rules_service =
      IOSRulesServiceFactory::GetForProfile(source_profile);
  CHECK(rules_service);

  Verdict verdict = rules_service->GetCopyRestrictedBySourceVerdict(source_url);

  if (verdict.level() == Rule::Level::kBlock) {
    std::move(callback).Run(CopyDecision::kBlock);
  } else {
    std::move(callback).Run(CopyDecision::kAllow);
  }
}

}  // namespace data_controls
