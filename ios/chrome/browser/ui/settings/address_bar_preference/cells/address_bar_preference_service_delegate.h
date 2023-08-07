// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_PREFERENCE_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_PREFERENCE_SERVICE_DELEGATE_H_

#import <Foundation/Foundation.h>

// A protocol implemented by delegates to handle address bar preference cell
// state.
@protocol AddressBarPreferenceServiceDelegate

// Sets the preference for address bar position to be on top.
- (void)didSelectTopAddressBarPreference;
// Sets the preference for address bar position to be on bottom.
- (void)didSelectBottomAddressBarPreference;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_CELLS_ADDRESS_BAR_PREFERENCE_SERVICE_DELEGATE_H_
