// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TIPS_PASSWORDS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TIPS_PASSWORDS_COMMANDS_H_

namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Commands related to the Magic Stack passwords tips.
@protocol TipsPasswordsCommands

// Shows the passwords tip for `identifier` (e.g. kSavePasswords,
// kAutofillPasswords).
- (void)showPasswordsTipForIdentifier:
    (segmentation_platform::TipIdentifier)identifier;

// Dismisses the passwords tip if currently displayed.
- (void)dismissPasswordsTip;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TIPS_PASSWORDS_COMMANDS_H_
