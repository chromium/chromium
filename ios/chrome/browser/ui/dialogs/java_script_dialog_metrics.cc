// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/dialogs/java_script_dialog_metrics.h"

#include "base/metrics/histogram_macros.h"

// Records a histogram for a dialog dismissal for |cause|.
void RecordDialogDismissalCause(IOSJavaScriptDialogDismissalCause cause) {
  UMA_HISTOGRAM_ENUMERATION("IOS.Dialogs.JavaScriptDialogClosed", cause);
}
