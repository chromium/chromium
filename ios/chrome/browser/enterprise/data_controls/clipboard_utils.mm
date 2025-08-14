// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/clipboard_utils.h"

#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

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

void IsCopyRestrictedByPolicy(
    const ActionContext& action_context,
    const ui::ClipboardMetadata& metadata,
    ProfileIOS* source_profile,  // Can be null if the source isn't Chrome
    ProfileIOS* destination_profile,
    web::WebState* webState,
    IsClipboardCopyAllowedCallbackIOS callback) {
  // TODO(crbug.com/438200537): This is the place holder for copy policy
  // evaluation API.
}

}  // namespace data_controls
