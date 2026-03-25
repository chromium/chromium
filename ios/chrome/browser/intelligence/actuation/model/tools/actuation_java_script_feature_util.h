// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_JAVA_SCRIPT_FEATURE_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_JAVA_SCRIPT_FEATURE_UTIL_H_

#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"

namespace base {
class Value;
}  // namespace base

// Processes a standard {success: bool, message: string} JS result.
void ParseJavaScriptResult(ActuationTool::ActuationCallback callback,
                           const base::Value* result);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_JAVA_SCRIPT_FEATURE_UTIL_H_
