// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_ENUMS_H_
#define IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_ENUMS_H_

namespace data_controls {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ClipboardSource)
enum class ClipboardSource {
  kEditMenu = 0,
  kClipboardAPI = 1,
  kCustomAction = 2,
  kMaxValue = kCustomAction,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IOSClipboardSource)

// Enum representing the type of clipboard action.
enum class ClipboardAction {
  kCopy,
  kCut,
  kPaste,
};

}  // namespace data_controls

#endif  // IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_CLIPBOARD_ENUMS_H_
