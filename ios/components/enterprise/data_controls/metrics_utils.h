// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_METRICS_UTILS_H_
#define IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_METRICS_UTILS_H_

#import "ios/components/enterprise/data_controls/clipboard_enums.h"

namespace data_controls {

// Records the source of a clipboard action.
void RecordClipboardSourceMetrics(ClipboardAction action,
                                  ClipboardSource source);

// Records the outcome of a clipboard action.
void RecordClipboardOutcomeMetrics(ClipboardAction action, bool allowed);

}  // namespace data_controls

#endif  // IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_METRICS_UTILS_H_
