// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_preference_service_delegate.h"

@protocol AddressBarPreferenceConsumer;

// The Mediator for address bar preference setting.
@interface AddressBarPreferenceMediator
    : NSObject <AddressBarPreferenceServiceDelegate>

- (instancetype)init;

// The consumer of the address bar preference mediator.
@property(nonatomic, weak) id<AddressBarPreferenceConsumer> consumer;

// Disconnects the address bar preference observation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ADDRESS_BAR_PREFERENCE_ADDRESS_BAR_PREFERENCE_MEDIATOR_H_
