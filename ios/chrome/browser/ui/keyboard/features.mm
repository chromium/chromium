// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/features.h"

#import <Foundation/Foundation.h>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kKeyboardShortcutsMenu,
             "KeyboardShortcutsMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Key for NSUserDefaults containing a bool indicating whether the next run
// should enable the keyboard shortcuts menu. This is used because building the
// menu can happen early in app initialization (when a hardware keyboard is
// connected prior to launch) and FeatureList is not yet available. Changing the
// `kEnableKeyboardShortcutsMenu` feature will always take effect after two cold
// starts after the feature has been changed on the server (once for the Finch
// configuration, and another for reading the stored value from NSUserDefaults).
NSString* const kEnableKeyboardShortcutsMenuForNextColdStart =
    @"EnableKeyboardShortcutsMenuForNextColdStart";

bool IsKeyboardShortcutsMenuEnabled() {
  if (@available(iOS 15.0, *)) {
    static bool keyboardShortcutsMenuEnabled =
        [[NSUserDefaults standardUserDefaults]
            boolForKey:kEnableKeyboardShortcutsMenuForNextColdStart];
    return keyboardShortcutsMenuEnabled;
  } else {
    return false;
  }
}

void SaveKeyboardShortcutsMenuEnabledForNextColdStart() {
  DCHECK(base::FeatureList::GetInstance());
  [[NSUserDefaults standardUserDefaults]
      setBool:base::FeatureList::IsEnabled(kKeyboardShortcutsMenu)
       forKey:kEnableKeyboardShortcutsMenuForNextColdStart];
}
