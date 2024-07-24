// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_VIEW_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_VIEW_H_

#import <UIKit/UIKit.h>

// Notification sent by the tab strip when its frame changes.
extern NSString* const kTabStripViewFrameDidChangeNotification;

// TabStripView does not lay out its own subviews, instead handing that off to a
// delegate.  This protocol is implemented by TabStripView delegates that are
// capable of handling layout.
// The TabStripViewLayoutDelegate also forward notification of change of trait
// collection that impact layout.
@protocol TabStripViewLayoutDelegate
// Called from `-layoutSubviews`.  Delegates should implement this method to
// layout subviews based on the current contentOffset of the tab strip.
// TabStripView does not perform any other subview layout.
- (void)layoutTabStripSubviews;
// Called from UIView `-traitCollectionDidChange:`.
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection;
@end

// View class for the tabstrip.  Contains one TabView per open tab and manages
// tab overflow behavior.
@interface TabStripView : UIScrollView

@property(nonatomic, weak) id<TabStripViewLayoutDelegate> layoutDelegate;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TAB_STRIP_VIEW_H_
