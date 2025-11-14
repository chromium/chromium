// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_METRICS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_METRICS_H_

// Name of the histogram that collects metrics of data control verdict for copy
// action.
extern const char kIOSWebStateDataControlsClipboardCopyVerdictHistogram[];

// Name of the histogram that collects metrics of data control verdict for paste
// action.
extern const char kIOSWebStateDataControlsClipboardPasteVerdictHistogram[];

// Name of the histogram that collects data of whether users ignore warning and
// continue the copy action.
extern const char
    kIOSWebStateDataControlsClipboardCopyClipboardWarningBypassedHistogram[];

// Name of the histogram that collects data of whether users ignore warning and
// continue the paste action.
extern const char
    kIOSWebStateDataControlsClipboardPasteClipboardWarningBypassedHistogram[];

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_METRICS_H_
