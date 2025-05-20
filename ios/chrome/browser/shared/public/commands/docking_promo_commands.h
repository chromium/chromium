// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_

// Commands to show app-wide Docking Promo(s).
@protocol DockingPromoCommands <NSObject>

// Show Docking Promo if conditions are met, or if `forced` is YES.
- (void)showDockingPromo:(BOOL)forced;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOCKING_PROMO_COMMANDS_H_
