// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_DELEGATE_H_

class GURL;

@protocol LensSelection <NSObject>

// If a polygon is necessary here, that's ok. I'm not sure why we would need
// this in Bling yet, so it's optional.
@property(nonatomic, readonly, assign) CGRect selectionRect;

// For any valid selection? Used to show in the bottom sheet header
@property(nonatomic, readonly, strong) UIImage* image;

// For text-based selections, maybe? To put in the multimodal omnibox
@property(nonatomic, readonly, copy) NSString* text;

@end

@protocol LensOverlaySelectionDelegate <NSObject>

/// May be called multiple times, once per selection.
- (void)selectionUI:(id)selectionUI
           performedSelection:(id<LensSelection>)selection
    constructedResultsPageURL:(GURL)resultsPageURL
               suggestSignals:(NSString*)iil;

/// May be called multiple times, up to once per selection.
- (void)selectionUI:(id)selectionUI
    encounteredError:(NSError*)error
       withSelection:(id<LensSelection>)selection;

// Called when the full image request has succeeded, so the user now has the
// option to select from server-detected regions.
- (void)selectionUISuccessfullyCompletedFullImageRequest:(id)selectionUI;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SELECTION_DELEGATE_H_
