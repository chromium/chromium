// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_CLIPBOARD_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_CLIPBOARD_UTILS_H_

#import "base/functional/callback_forward.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/web/public/web_state.h"
#import "ui/base/clipboard/clipboard_metadata.h"

class ProfileIOS;
class GURL;

namespace data_controls {

// A struct representing the verdict of a paste action from a policy
// perspective.
struct PastePolicyVerdict {
  // The final `Verdict` for the paste action.
  Verdict verdict;
  // A boolean indicating whether a warning or blocking dialog was triggered by
  // the source's policy configuration.
  bool dialog_triggered_by_source;
};

// This function checks if a paste is allowed to proceed according to the
// following policies:
// - OnBulkDataEntryEnterpriseConnector
// - DataControlsRules
PastePolicyVerdict IsPasteAllowedByPolicy(
    const GURL& source_url,
    const GURL& destination_url,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile);

// A struct representing the verdicts of a copy action from a policy
// perspective.
struct CopyPolicyVerdicts {
  // The verdict for the copy action itself.
  Verdict copy_action_verdict;
  // A blooean indicating whether the data should be copied to os clipboard.
  bool copy_to_os_clipbord;
};

// This function checks if a copy action is allowed by the "DataControlsRules"
// policy.
CopyPolicyVerdicts IsCopyAllowedByPolicy(
    const GURL& source_url,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile  // Must be non-null.
);

// This function checks if a share action is allowed by the "DataControlsRules"
// policy.
Verdict IsShareAllowedByPolicy(const GURL& source_url,
                               ProfileIOS* source_profile  // Must be non-null.
);

// Reports a paste action if applicable.
void MaybeReportDataControlsPaste(const GURL& source_url,
                                  const GURL& destination_url,
                                  ProfileIOS* source_profile,
                                  ProfileIOS* destination_profile,
                                  const ui::ClipboardMetadata& metadata,
                                  const Verdict& verdict,
                                  bool bypassed = false);

// Reports a copy action if applicable.
void MaybeReportDataControlsCopy(const GURL& source_url,
                                 ProfileIOS* source_profile,
                                 const ui::ClipboardMetadata& metadata,
                                 const Verdict& verdict,
                                 bool bypassed = false);

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_CLIPBOARD_UTILS_H_
