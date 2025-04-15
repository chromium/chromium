// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_COORDINATOR_H_

#include <memory>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class AutocompleteController;
@class OmniboxAutocompleteController;
@protocol OmniboxKeyboardDelegate;
@protocol OmniboxPopupPresenterDelegate;
class OmniboxPopupViewIOS;
@protocol ToolbarOmniboxConsumer;

/// Coordinator for the Omnibox Popup.
@interface OmniboxPopupCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                    autocompleteController:
                        (AutocompleteController*)autocompleteController
                                 popupView:
                                     (std::unique_ptr<OmniboxPopupViewIOS>)
                                         popupView
             omniboxAutocompleteController:
                 (OmniboxAutocompleteController*)omniboxAutocompleteController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

/// Positioner for the popup.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> presenterDelegate;
/// Whether this coordinator has results to show.
@property(nonatomic, assign, readonly) BOOL hasResults;
/// Whether the popup is open.
@property(nonatomic, assign, readonly) BOOL isOpen;

/// Object implementing OmniboxKeyboardDelegate in OmniboxPopupCoordinator.
@property(nonatomic, weak, readonly) id<OmniboxKeyboardDelegate>
    KeyboardDelegate;

// Returns the toolbar omnibox consumer.
- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer;

/// Toggle visibility of the omnibox debugger view.
- (void)toggleOmniboxDebuggerView;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_COORDINATOR_POPUP_OMNIBOX_POPUP_COORDINATOR_H_
