// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_CONSTANTS_H_

#import "base/time/time.h"

namespace autofill_ui_constants {

// Delay before allowing selecting a suggestion for filling. This helps
// preventing clickjacking by giving more time to the user to understand what
// the UI does.
const base::TimeDelta kSelectSuggestionDelay = base::Milliseconds(500);

// The delay between showing the confirmation and dismissing the progress
// dialog.
const base::TimeDelta kProgressDialogConfirmationDismissDelay =
    base::Seconds(1);

}  // namespace autofill_ui_constants

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_UI_CONSTANTS_H_
