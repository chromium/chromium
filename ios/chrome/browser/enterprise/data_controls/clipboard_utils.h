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

namespace data_controls {

// Callback used with the `IsPasteAllowedByPolicy()` method.  If the
// paste is not allowed, the paste action will be intercepted.  Otherwise, the
// data that should be pasted is passed in.
using IsClipboardPasteAllowedCallbackIOS = base::OnceCallback<void()>;

// Callback used with the `IsCopyAllowedByPolicy()` method.
// If the copy is allowed, the actual data is expected to be copied. Otherwise,
// `replacement_data` should be written in plaintext to the clipboard.
using IsClipboardCopyAllowedCallbackIOS =
    base::OnceCallback<void(const ui::ClipboardFormatType& type,
                            std::optional<std::u16string> replacement_data)>();

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
// written to the OS clipboard according to the following policy:
// - DataControlsRules
//
// If the copy is not allowed, `callback` is called with a replacement string
// that should instead be put into the OS clipboard.
void IsCopyAllowedByPolicy(
    const ActionContext& action_context,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile,
    web::WebState* webState,
    IsClipboardCopyAllowedCallbackIOS callback);

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_UTILS_H_
