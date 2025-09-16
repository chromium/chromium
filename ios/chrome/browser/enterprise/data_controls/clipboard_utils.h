// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_UTILS_H_

#import "base/functional/callback.h"
#import "base/functional/callback_forward.h"
#import "components/enterprise/data_controls/core/browser/action_context.h"
#import "ios/web/public/web_state.h"
#import "ui/base/clipboard/clipboard_metadata.h"

class ProfileIOS;
class GURL;

namespace data_controls {

// Enum representing the decision made by Data Controls for a copy operation.
enum class CopyDecision {
  // The copy is allowed, and the data can be copied as-is.
  kAllow,
  // The copy is allowed, but the data should be marked as controlled by the
  // policy.
  kAllowAndProtect,
  // The copy is blocked.
  kBlock,
};

// Callback used with the `IsPasteAllowedByPolicy()` method.  If the
// paste is not allowed, the paste action will be intercepted.  Otherwise, the
// data that should be pasted is passed in.
using IsClipboardPasteAllowedCallbackIOS = base::OnceCallback<void()>;

// Callback used with the `IsCopyAllowedByPolicy()` method.
using IsClipboardCopyAllowedCallbackIOS =
    base::OnceCallback<void(CopyDecision)>;

// This function checks if a paste is allowed to proceed according to the
// following policies:
// - OnBulkDataEntryEnterpriseConnector
// - DataControlsRules
//
// This function will always call `callback` after policies are evaluated with
// true if the paste is allowed to proceed and false if it is not. However, if
// policies indicate the paste action should receive a bypassable warning, then
// `callback` will only be called after the user makes the decision to bypass
// the warning or not. As such, callers should be careful not to bind data that
// could become dangling as `callback` is not guaranteed to run synchronously.
void IsPasteAllowedByPolicy(
    const ActionContext& action_context,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile,
    web::WebState* webState,
    IsClipboardPasteAllowedCallbackIOS callback);

// This function checks if data copied from a browser tab is allowed to be
// written to the clipboard according to the following policy:
// - DataControlsRules
void IsCopyAllowedByPolicy(const GURL& source_url,
                           const ui::ClipboardMetadata& metadata,
                           ProfileIOS* source_profile,  // Must be non-null.
                           web::WebState* webState,
                           IsClipboardCopyAllowedCallbackIOS callback);

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_UTILS_H_
