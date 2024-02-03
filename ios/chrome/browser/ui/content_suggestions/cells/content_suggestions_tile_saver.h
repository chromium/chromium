// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_SAVER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_SAVER_H_

#import <Foundation/Foundation.h>

#include "components/ntp_tiles/ntp_tile.h"

@class FaviconAttributesProvider;
@class NTPTile;

// These functions are used to save the content suggestions tiles (favicon and
// name) offline for the use of the content widget. The most visited info and
// icon fallback data are saved to userdefaults. The favicons are saved to a
// shared directory.
namespace content_suggestions_tile_saver {

// Saves the most visited sites to disk with icons in `favicons_directory`,
// using `favicon_fetcher` to get the favicons.
void SaveMostVisitedToDisk(const ntp_tiles::NTPTilesVector& most_visited_data,
                           FaviconAttributesProvider* favicon_provider,
                           NSURL* favicons_directory);

// Read the current saved most visited sites from disk.
NSDictionary* ReadSavedMostVisited();

// Fetches the updated favicon for a single site and saves it in
// `favicons_directory`.
void UpdateSingleFavicon(const GURL& site_url,
                         FaviconAttributesProvider* favicon_provider,
                         NSURL* favicons_directory);

}  // namespace content_suggestions_tile_saver

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_TILE_SAVER_H_
