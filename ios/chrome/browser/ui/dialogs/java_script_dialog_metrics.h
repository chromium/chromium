// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_METRICS_H_
#define IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_METRICS_H_

// Possible reasons for JavaScript dialog dismissals.  These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class IOSJavaScriptDialogDismissalCause {
  // The tab was closed while a JavaScript dialog was displayed.
  kClosure = 0,
  // The user taps the OK, Cancel, or Suppress Dialogs button.
  kUser = 1,
  // The dialog was blocked by the Suppress Dialog option.
  kBlocked = 2,
  // The dialog was closed due to navigation.
  kNavigation = 3,

  kMaxValue = kNavigation,
};

// Records a histogram for a dialog dismissal for |cause|.
void RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause cause);

#endif  // IOS_CHROME_BROWSER_UI_DIALOGS_JAVA_SCRIPT_DIALOG_METRICS_H_
