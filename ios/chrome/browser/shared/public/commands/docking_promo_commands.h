// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_

// Trigger sources for the docking promo.
enum class DockingPromoTrigger {
  kTriggerUnset,
  kTipsModule,
  kSetUpList,
  kPromosManager,
  kFRE,
};

// Commands to show app-wide Docking Promo(s).
@protocol DockingPromoCommands <NSObject>

// Show Docking Promo if conditions are met, or if `forced` is YES.
- (void)showDockingPromoWithTrigger:(DockingPromoTrigger)trigger;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_
