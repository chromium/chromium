// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_

#import <UIKit/UIKit.h>

#include <set>

#import "ios/web/public/web_state_id.h"

@protocol TabPickerLogger;
@protocol TabPickerSnackbarPresenter;

// Callback which returns the IDs of tabs that were selected by the user and
// which of those have persisted tab context available.
typedef void (^TabPickerCompletionBlock)(std::set<web::WebStateID> selectedIDs,
                                         std::set<web::WebStateID> cachedIDs);

// Contains parameters used to configure the tab picker.
@interface TabPickerParams : NSObject

// The view controller that should present the tab picker. If nil, the root
// browser view controller will be used. If the presenting view controller has
// presented view controllers, the tab picker will be presented on the top-most
// view controller.
@property(nonatomic, weak) UIViewController* baseViewController;

// The maximum number of tabs that can be attached. Default value is 10.
@property(nonatomic, assign) NSUInteger maxTabAttachmentCount;

// The IDs of the tabs that should be preselected. If empty, no tabs will be
// preselected.
@property(nonatomic, assign) std::set<web::WebStateID> preselectedWebStateIDs;

// Whether PDF extraction is enabled for the tab picker.
@property(nonatomic, assign) BOOL PDFEnabled;

// The presenter that will display snackbar messages. This should stay non-nil
// for the duration of the tab picker's lifecycle.
@property(nonatomic, weak, readonly) id<TabPickerSnackbarPresenter>
    snackbarPresenter;

// Optional. Logger for tab picker events.
@property(nonatomic, weak) id<TabPickerLogger> logger;

// Initializes the parameters with the required `snackbarPresenter`.
- (instancetype)initWithSnackbarPresenter:
    (id<TabPickerSnackbarPresenter>)snackbarPresenter NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@protocol TabPickerCommands <NSObject>

// Shows the tab picker UI configured with the given non-nil `params`.
// `completion` is called when the user triggers the tab picker's dismissal
// only if the selected tabs have changed.
- (void)showTabPickerWithParams:(TabPickerParams*)params
                     completion:(TabPickerCompletionBlock)completion;

// Hides the tab picker UI.
- (void)hideTabPicker;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_
