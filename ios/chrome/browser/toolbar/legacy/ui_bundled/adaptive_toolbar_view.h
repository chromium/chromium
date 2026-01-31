// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class LegacyToolbarButton;
@class ToolbarProgressBar;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;
enum class ToolbarTabGroupState;

// Protocol defining the interface for interacting with a view of the adaptive
// toolbar.
@protocol AdaptiveToolbarView <NSObject>
// Button to cancel the edit of the location bar.
@property(nonatomic, strong, readonly) UIButton* cancelButton;
// Property to get all the buttons in this view.
@property(nonatomic, strong, readonly)
    NSArray<LegacyToolbarButton*>* allButtons;

// Progress bar displayed below the toolbar.
@property(nonatomic, strong, readonly) ToolbarProgressBar* progressBar;
// Button to navigate back.
@property(nonatomic, strong, readonly) LegacyToolbarButton* backButton;
// Buttons to navigate forward.
@property(nonatomic, strong, readonly) LegacyToolbarButton* forwardButton;
// Button to display the TabGrid.
@property(nonatomic, strong, readonly) ToolbarTabGridButton* tabGridButton;
// Button to stop the loading of the page.
@property(nonatomic, strong, readonly) LegacyToolbarButton* stopButton;
// Button to reload the page.
@property(nonatomic, strong, readonly) LegacyToolbarButton* reloadButton;
// Button to display the share menu.
@property(nonatomic, strong, readonly) LegacyToolbarButton* shareButton;
// Button to display the tools menu.
@property(nonatomic, strong, readonly) LegacyToolbarButton* toolsMenuButton;
// Button to create a new tab.
@property(nonatomic, strong, readonly) LegacyToolbarButton* openNewTabButton;
// Separator between the toolbar and the content.
@property(nonatomic, strong, readonly) UIView* separator;

// Container for the location bar.
@property(nonatomic, strong, readonly) UIView* locationBarContainer;
// The height of `locationBarContainer`.
@property(nonatomic, strong, readonly)
    NSLayoutConstraint* locationBarContainerHeight;
// Button taking the full size of the toolbar. Expands the toolbar when tapped.
@property(nonatomic, strong, readonly) UIButton* collapsedToolbarButton;

// Height of the location bar.
@property(nonatomic, assign) CGFloat locationBarHeight;

// Sets the location bar view containing the omnibox.
- (void)setLocationBarView:(UIView*)locationBarView;

// Updates the toolbar for the given TabGroup state.
- (void)updateTabGroupState:(ToolbarTabGroupState)tabGroupState;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_LEGACY_UI_BUNDLED_ADAPTIVE_TOOLBAR_VIEW_H_
