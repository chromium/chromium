// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ReaderModeOptionsControlsView;
@protocol ReaderModeOptionsCommands;
@protocol ReaderModeOptionsMutator;

// View controller for the reader mode options.
@interface ReaderModeOptionsViewController : UINavigationController

// The view that contains the controls for the Reader Mode options.
@property(nonatomic, readonly) ReaderModeOptionsControlsView* controlsView;

@property(nonatomic, weak) id<ReaderModeOptionsMutator> mutator;

@property(nonatomic, weak) id<ReaderModeOptionsCommands>
    readerModeOptionsHandler;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNavigationBarClass:(Class)navigationBarClass
                              toolbarClass:(Class)toolbarClass NS_UNAVAILABLE;
- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
    NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_VIEW_CONTROLLER_H_
