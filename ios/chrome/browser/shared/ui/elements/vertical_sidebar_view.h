// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_H_

#import <UIKit/UIKit.h>

// A full-height column of glass pills, drawn in the system vertical bar column
// (iPhone Duo folded). Each group of views is drawn as one pill; the views are
// laid out as fixed-size cells, top to bottom. A 1pt separator runs along
// the inner edge, facing the content. Only the pills receive touches. Sized by
// its frame only.
@interface VerticalSidebarView : UIView

// The column's outer edge, i.e. the screen edge it's on: `UIRectEdgeLeft` or
// `UIRectEdgeRight` (the default). Set by the owner, which knows where it
// placed the column. The pills sit on the system's bar axis next to this edge,
// and the separator runs along the opposite one.
@property(nonatomic, assign) UIRectEdge outerEdge;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Sets the pill groups. `topGroups` stack down from the top of the column's
// area free of system obstructions (e.g. below the vertical status bar);
// `bottomGroups` stack up from its bottom (e.g. above the camera), the last
// group closest to the bottom. Empty groups are skipped.
//
// That area is about 360pt tall in folded landscape, enough for up to 6 views
// in up to 3 pills. Pills that don't fit extend past it, over the status bar
// or the camera, but stay in the column and never overlap.
//
// About the views:
// - Give the sidebar views dedicated to it, each in only one group. It keeps
//   them, so update them (state, visibility) in place.
// - Each view keeps its own size, so it must have one (intrinsic or
//   constrained). It's centered in a cell, which keeps its place when the view
//   is hidden.
// - Touches anywhere in a view's cell go to that view.
// - Setting new groups replaces the previous pills and their views.
- (void)setTopGroups:(NSArray<NSArray<UIView*>*>*)topGroups
        bottomGroups:(NSArray<NSArray<UIView*>*>*)bottomGroups;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_VERTICAL_SIDEBAR_VIEW_H_
