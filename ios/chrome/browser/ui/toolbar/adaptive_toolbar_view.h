// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class MDCProgressView;
@class ToolbarButton;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;
enum class ToolbarTabGridButtonStyle;

// Protocol defining the interface for interacting with a view of the adaptive
// toolbar.
@protocol AdaptiveToolbarView<NSObject>

// Property to get all the buttons in this view.
@property(nonatomic, strong, readonly) NSArray<ToolbarButton*>* allButtons;

// Progress bar displayed below the toolbar.
@property(nonatomic, strong, readonly) MDCProgressView* progressBar;
// Button to navigate back.
@property(nonatomic, strong, readonly) ToolbarButton* backButton;
// Buttons to navigate forward.
@property(nonatomic, strong, readonly) ToolbarButton* forwardButton;
// Button to display the TabGrid.
@property(nonatomic, strong, readonly) ToolbarTabGridButton* tabGridButton;
// Button to stop the loading of the page.
@property(nonatomic, strong, readonly) ToolbarButton* stopButton;
// Button to reload the page.
@property(nonatomic, strong, readonly) ToolbarButton* reloadButton;
// Button to display the share menu.
@property(nonatomic, strong, readonly) ToolbarButton* shareButton;
// Button to display the tools menu.
@property(nonatomic, strong, readonly) ToolbarButton* toolsMenuButton;
// Button to create a new tab.
@property(nonatomic, strong, readonly) ToolbarButton* openNewTabButton;
// Separator between the toolbar and the content.
@property(nonatomic, strong, readonly) UIView* separator;

// Container for the location bar.
@property(nonatomic, strong, readonly) UIView* locationBarContainer;
// The height of `locationBarContainer`.
@property(nonatomic, strong, readonly)
    NSLayoutConstraint* locationBarContainerHeight;
// Button taking the full size of the toolbar. Expands the toolbar when tapped.
@property(nonatomic, strong, readonly) UIButton* collapsedToolbarButton;

// Sets the location bar view containing the omnibox.
- (void)setLocationBarView:(UIView*)locationBarView;

// Sets the style on the Tab Grid button.
- (void)setTabGridButtonStyle:(ToolbarTabGridButtonStyle)tabGridButtonStyle;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_TOOLBAR_VIEW_H_
