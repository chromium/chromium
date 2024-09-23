// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_PAGE_CONTROL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_PAGE_CONTROL_H_

#include <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

// Custom control events for the actions on the TabGridPageControl.
extern UIControlEvents TabGridPageChangeByTapEvent;
extern UIControlEvents TabGridPageChangeByDragEvent;

// A three-sectioned control for selecting a page in the tab grid.
// A "slider" is positioned over the section for the selected page.
// This is a fixed-size control; it's an error to set  or change its size.
// The sections are arranged in leading-to-trailing order:
//   incognito tabs, regular tabs, and Recent Tabs or Tab Groups.
//
// Dragging the slider will change the value of the control's `sliderPosition`
// property, and will trigger any UIControlEventValueChanged actions. Once a
// drag is completed, the UIControlEventTouchUpInside actions are triggered.
//
// Tapping on sections of the slider will change the value of the control's
// `selectedPage` property and will trigger any UIControlEventTouchUpInside
// actions.
@interface TabGridPageControl : UIControl

// The currently selected page in the control. When this value is changed by
// a user interaction, the UIControlEventValueChanged actions are sent.
// Setting this property will update the position of the slider without
// animation. When an instance of this control is created, this value defaults
// to TabGridPageRegularTabs.
@property(nonatomic, assign) TabGridPage selectedPage;

// The position of the slider, from 0.0 to 1.0, where 0.0 is as far as possible
// to the leading side of the control, and 1.0 is as far as possible to the
// trailing side of the control. Setting this property will update the position
// of the slider without animation. Setting a value below 0.0 or above 1.0 will
// set 0.0 or 1.0 instead.
// Setting this property may change the `selectedPage` property of the receiver,
// but will not cause any UIControl actions to be sent.
@property(nonatomic, assign) CGFloat sliderPosition;

// The number of tabs that the control should display in the appropriate
// sections.
// Numbers less than 1 are not displayed.
// Numbers greater than 99 are displayed as ':-)'.
@property(nonatomic, assign) NSUInteger tabCount;

// Create and return a new instance of this control. This is the preferred way
// to create instances of this class.
+ (instancetype)pageControl;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Set `selectedPage` as the selected page. If `animated` is YES, the
// position change of the slider will be animated.
- (void)setSelectedPage:(TabGridPage)selectedPage animated:(BOOL)animated;

// Updates the appearance of the control, based on whether the content below it
// is `scrolledToEdge` or not.
- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge;

// Highlights (put a blue background) the last element of the page control.
- (void)highlightLastPageControl;
// Removes the highlight on the last page control, if there is one.
- (void)resetLastPageControlHighlight;

// Returns the frame of the last segment, in window coordinates.
- (CGRect)lastSegmentFrame;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_PAGE_CONTROL_H_
