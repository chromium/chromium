// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_MODEL_TIPS_PREFS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_MODEL_TIPS_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace tips_prefs {

// Returns `true` if the Tips module is disabled.
bool IsTipsInMagicStackDisabled(PrefService* prefs);

// Disables the Tips module.
void DisableTipsInMagicStack(PrefService* prefs);

}  // namespace tips_prefs

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TIPS_MODEL_TIPS_PREFS_H_
