// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

// Feature to enable Reader Mode omnibox entry point in the US country code.
BASE_DECLARE_FEATURE(kEnableReaderModeOmniboxEntryPointInUS);

// Returns whether the omnibox entrypoint is enabled.
bool IsReaderModeOmniboxEntryPointEnabled();

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_FEATURES_H_
