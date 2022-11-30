// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer.h"

@class TabStripMediator;
@protocol TabFaviconDataSource;
@protocol TabStripConsumerDelegate;

// ViewController for the TabStrip. This ViewController is contained by
// BrowserViewController. This TabStripViewController is responsible for
// responding to the different updates in the tabstrip view.
@interface TabStripViewController
    : UICollectionViewController <TabStripConsumer>

@property(nonatomic, weak) id<TabFaviconDataSource> faviconDataSource;
@property(nonatomic, weak) id<TabStripConsumerDelegate> delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_VIEW_CONTROLLER_H_
