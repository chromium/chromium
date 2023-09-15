// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/memory/scoped_refptr.h"

@class RecipientInfoForIOSDisplay;

@protocol FamilyPickerConsumer;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// This mediator passes display information about potential password sharing
// recipients of the user to its consumer.
@interface FamilyPickerMediator : NSObject

- (instancetype)
        initWithRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<FamilyPickerConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PICKER_MEDIATOR_H_
