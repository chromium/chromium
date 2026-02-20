// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_

class PrefService;

namespace gemini {
// TODO(crbug.com/485892276): Move ProfilePrefs registration here.

// Enterprise policies allow for Gemini usage within Chrome.
bool GeminiAllowedByPolicy(PrefService* prefs);
bool GenAiAllowedByEnterprise(PrefService* prefs);
bool GeminiAllowedByEnterprise(PrefService* prefs);
}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_PREFS_H_
