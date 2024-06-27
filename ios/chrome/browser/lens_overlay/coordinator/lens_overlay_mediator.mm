// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import "url/gurl.h"

@implementation LensOverlayMediator

- (void)startWithSnapshot:(UIImage*)snapshot {
  [self.snapshotConsumer loadSnapshot:snapshot];
}

#pragma mark - LensOverlaySelectionDelegate

- (void)selectionUI:(id)selectionUI
           performedSelection:(id<LensSelection>)selection
    constructedResultsPageURL:(GURL)resultsPageURL
               suggestSignals:(NSString*)iil {
  [self.resultConsumer loadResultsURL:resultsPageURL];
}

- (void)selectionUI:(id)selectionUI
    encounteredError:(NSError*)error
       withSelection:(id<LensSelection>)selection {
}

- (void)selectionUISuccessfullyCompletedFullImageRequest:(id)selectionUI {
}

@end
