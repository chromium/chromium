// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_LIVE_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_LIVE_UTILS_H_

class ProfileIOS;

namespace gemini {

// Updates the Gemini Live indicator in the location bar for all regular
// browsers of `profile` to reflect `in_live_mode`. No-op if Gemini Live is
// disabled or `profile` is null.
void UpdateGeminiLiveIconVisibility(ProfileIOS* profile, bool in_live_mode);

}  // namespace gemini

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_GEMINI_LIVE_UTILS_H_
