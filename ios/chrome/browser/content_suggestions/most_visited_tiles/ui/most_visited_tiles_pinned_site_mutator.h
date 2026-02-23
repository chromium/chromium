// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PINNED_SITE_MUTATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PINNED_SITE_MUTATOR_H_

/// Whether the pinned site is successfully mutated.
enum class PinnedSiteMutationResult {
  /// Pinned site added or edited.
  kSuccess,
  /// The URL input has an invalid scheme.
  kURLInvalid,
  /// The URL is already pinned.
  kURLExisted,
};

/// Mutator object to handle creating and editing pinned sites in the most
/// visited tiles.
@protocol MostVisitedTilesPinnedSiteMutator

/// Adds a new pinned site with `URL` and `title`.
- (PinnedSiteMutationResult)addPinnedSiteWithTitle:(NSString*)title
                                               URL:(NSString*)URL;

/// Update the existing pinned site for `oldURL`  with  an updated `newURL` and
/// `title`.
- (PinnedSiteMutationResult)editPinnedSiteForURL:(NSString*)oldURL
                                       withTitle:(NSString*)title
                                             URL:(NSString*)newURL;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_PINNED_SITE_MUTATOR_H_
