// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/enterprise/data_controls/metrics_utils.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"

namespace data_controls {

namespace {

// Returns the string representation of the clipboard action.
std::string GetClipboardActionString(ClipboardAction action) {
  switch (action) {
    case ClipboardAction::kCopy:
      return "Copy";
    case ClipboardAction::kCut:
      return "Cut";
    case ClipboardAction::kPaste:
      return "Paste";
  }
  NOTREACHED();
}

}  // namespace

void RecordClipboardSourceMetrics(ClipboardAction action,
                                  ClipboardSource source) {
  base::UmaHistogramEnumeration(
      base::StrCat({"IOS.WebState.Clipboard.", GetClipboardActionString(action),
                    ".Source"}),
      source);
}

void RecordClipboardOutcomeMetrics(ClipboardAction action, bool allowed) {
  base::UmaHistogramBoolean(
      base::StrCat({"IOS.WebState.Clipboard.", GetClipboardActionString(action),
                    ".Outcome"}),
      allowed);
}

}  // namespace data_controls
