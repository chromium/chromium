// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol MostVisitedTilesPinnedSiteMutator;

/// Modal for user to create or edit a pinned site in the most visited tiles.
@interface PinnedSiteFormViewController : UIViewController

/// Mutator object that handles pinned site creation and edits.
@property(nonatomic, weak) id<MostVisitedTilesPinnedSiteMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_
