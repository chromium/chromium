// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

@protocol ManualFillAllPlusAddressCoordinatorDelegate;
@protocol ManualFillContentInjector;
@protocol ManualFillPlusAddressConsumer;
@protocol PlusAddressListNavigator;

class FaviconLoader;
class GURL;

namespace plus_addresses {
class PlusAddressService;
}

// Delegate for the ManualFillPlusAddressMediator.
@protocol ManualFillPlusAddressMediatorDelegate

// The mediator will attempt to inject content.
- (void)manualFillPlusAddressMediatorWillInjectContent;

@end

// Responsible for fetching plus addresses relevant for the manual fill view.
@interface ManualFillPlusAddressMediator
    : NSObject <TableViewFaviconDataSource, UISearchResultsUpdating>

// The consumer for plus address updates. Setting it will trigger the consumer
// methods with the current data.
@property(nonatomic, weak) id<ManualFillPlusAddressConsumer> consumer;
// The delegate in charge of using the content selected by the user.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;
// The delegate in charge of navigation.
@property(nonatomic, weak) id<PlusAddressListNavigator> navigator;
// The delegate for this object.
@property(nonatomic, weak) id<ManualFillPlusAddressMediatorDelegate> delegate;

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                   plusAddressService:
                       (plus_addresses::PlusAddressService*)plusAddressService
                                  URL:(const GURL&)URL
                       isOffTheRecord:(BOOL)isOffTheRecord
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Fetches and stores all plus addresses. Should be called before the consumer
// for the select plus address sheet so that the consumer uses it.
- (void)fetchAllPlusAddresses;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PLUS_ADDRESS_MEDIATOR_H_
