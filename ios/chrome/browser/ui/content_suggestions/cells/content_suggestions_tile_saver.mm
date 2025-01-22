// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_saver.h"

#import "base/functional/bind.h"
#import "base/hash/md5.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ntp_tile/ntp_tile.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "net/base/apple/url_conversions.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/model/model_swift.h"
#endif

namespace content_suggestions_tile_saver {

// Write the `most_visited_sites` to disk.
void WriteSavedMostVisited(NSDictionary<NSURL*, NTPTile*>* most_visited_sites);

// Checks if every site in `tiles` has had its favicons fetched. If so, writes
// the info to disk, saving the favicons to `favicons_directory`.
void WriteToDiskIfComplete(NSDictionary<NSURL*, NTPTile*>* tiles,
                           NSURL* favicons_directory);

// Gets a name for the favicon file.
NSString* GetFaviconFileName(const GURL& url);

// If the sites currently saved include one with `tile`'s url, replace it by
// `tile`.
void WriteSingleUpdatedTileToDisk(NTPTile* tile);

// Get the favicons using `favicon_provider` and writes them to disk.
void GetFaviconsAndSave(const ntp_tiles::NTPTilesVector& most_visited_data,
                        FaviconAttributesProvider* favicon_provider,
                        NSURL* favicons_directory);

// Updates the list of tiles that must be displayed in the content suggestion
// widget.
void UpdateTileList(const ntp_tiles::NTPTilesVector& most_visited_data);

// Deletes icons contained in `favicons_directory` and corresponding to no URL
// in `most_visited_data`.
void ClearOutdatedIcons(const ntp_tiles::NTPTilesVector& most_visited_data,
                        NSURL* favicons_directory);

}  // namespace content_suggestions_tile_saver

namespace content_suggestions_tile_saver {

void UpdateTileList(const ntp_tiles::NTPTilesVector& most_visited_data) {
  NSMutableDictionary<NSURL*, NTPTile*>* tiles =
      [[NSMutableDictionary alloc] init];
  NSDictionary<NSURL*, NTPTile*>* old_tiles = ReadSavedMostVisited();
  for (size_t i = 0; i < most_visited_data.size(); i++) {
    const ntp_tiles::NTPTile& ntp_tile = most_visited_data[i];
    NSURL* ns_url = net::NSURLWithGURL(ntp_tile.url);
    if (!ns_url) {
      // If the URL for a particular tile is invalid, skip including it.
      continue;
    }
    NTPTile* tile =
        [[NTPTile alloc] initWithTitle:base::SysUTF16ToNSString(ntp_tile.title)
                                   URL:ns_url
                              position:i];
    tile.faviconFileName = GetFaviconFileName(ntp_tile.url);
    NTPTile* old_tile = [old_tiles objectForKey:ns_url];
    if (old_tile) {
      // Keep fallback data.
      tile.fallbackMonogram = old_tile.fallbackMonogram;
      tile.fallbackTextColor = old_tile.fallbackTextColor;
      tile.fallbackIsDefaultColor = old_tile.fallbackIsDefaultColor;
      tile.fallbackBackgroundColor = old_tile.fallbackBackgroundColor;
    }
    [tiles setObject:tile forKey:tile.URL];
  }
  WriteSavedMostVisited(tiles);
}

NSString* GetFaviconFileName(const GURL& url) {
  return [base::SysUTF8ToNSString(base::MD5String(url.spec()))
      stringByAppendingString:@".png"];
}

void GetFaviconsAndSave(const ntp_tiles::NTPTilesVector& most_visited_data,
                        FaviconAttributesProvider* favicon_provider,
                        NSURL* favicons_directory) {
  for (size_t i = 0; i < most_visited_data.size(); i++) {
    const GURL& gurl = most_visited_data[i].url;
    UpdateSingleFavicon(gurl, favicon_provider, favicons_directory);
  }
}

void ClearOutdatedIcons(const ntp_tiles::NTPTilesVector& most_visited_data,
                        NSURL* favicons_directory) {
  NSMutableSet<NSString*>* allowed_files_name = [[NSMutableSet alloc] init];
  for (size_t i = 0; i < most_visited_data.size(); i++) {
    const ntp_tiles::NTPTile& ntp_tile = most_visited_data[i];
    NSString* favicon_file_name = GetFaviconFileName(ntp_tile.url);
    [allowed_files_name addObject:favicon_file_name];
  }
  [[NSFileManager defaultManager] createDirectoryAtURL:favicons_directory
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  NSArray<NSURL*>* existing_files = [[NSFileManager defaultManager]
        contentsOfDirectoryAtURL:favicons_directory
      includingPropertiesForKeys:nil
                         options:0
                           error:nil];
  for (NSURL* file : existing_files) {
    if (![allowed_files_name containsObject:[file lastPathComponent]]) {
      [[NSFileManager defaultManager] removeItemAtURL:file error:nil];
    }
  }
}

void SaveMostVisitedToDisk(const ntp_tiles::NTPTilesVector& most_visited_data,
                           FaviconAttributesProvider* favicon_provider,
                           NSURL* favicons_directory) {
  if (favicons_directory == nil) {
    return;
  }
  UpdateTileList(most_visited_data);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ClearOutdatedIcons, most_visited_data,
                     favicons_directory),
      base::BindOnce(
          ^(const ntp_tiles::NTPTilesVector& inner_most_visited_data) {
            GetFaviconsAndSave(inner_most_visited_data, favicon_provider,
                               favicons_directory);
          },
          most_visited_data));
}

void WriteSingleUpdatedTileToDisk(NTPTile* tile) {
  NSMutableDictionary* tiles = [ReadSavedMostVisited() mutableCopy];
  if (![tiles objectForKey:tile.URL]) {
    return;
  }
  [tiles setObject:tile forKey:tile.URL];
  WriteSavedMostVisited(tiles);
}

// Updates the Shortcut's widget with the user's current most visited sites
void UpdateShortcutsWidget() {
#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
  [WidgetTimelinesUpdater reloadTimelinesOfKind:@"ShortcutsWidget"];
#endif
}

void WriteSavedMostVisited(NSDictionary<NSURL*, NTPTile*>* most_visited_data) {
  NSDate* last_modification_date = NSDate.date;
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:most_visited_data
                                       requiringSecureCoding:NO
                                                       error:&error];
  if (!data || error) {
    DLOG(WARNING) << "Error serializing most visited: "
                  << base::SysNSStringToUTF8([error description]);
    return;
  }

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();

  [sharedDefaults setObject:data forKey:app_group::kSuggestedItems];
  [sharedDefaults setObject:last_modification_date
                     forKey:app_group::kSuggestedItemsLastModificationDate];
  UpdateShortcutsWidget();
}

NSDictionary* ReadSavedMostVisited() {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver = [[NSKeyedUnarchiver alloc]
      initForReadingFromData:[sharedDefaults
                                 objectForKey:app_group::kSuggestedItems]
                       error:&error];
  if (!unarchiver || error) {
    DLOG(WARNING) << "Error creating unarchiver for most visited: "
                  << base::SysNSStringToUTF8([error description]);
    return [[NSMutableDictionary alloc] init];
  }

  unarchiver.requiresSecureCoding = NO;
  return [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
}

void UpdateSingleFavicon(const GURL& site_url,
                         FaviconAttributesProvider* favicon_provider,
                         NSURL* favicons_directory) {
  NSURL* siteNSURL = net::NSURLWithGURL(site_url);

  void (^faviconAttributesBlock)(FaviconAttributes*) =
      ^(FaviconAttributes* attributes) {
        if (attributes.faviconImage) {
          // Update the available icon.
          // If we have a fallback icon, do not remove it. The favicon will have
          // priority, and should anything happen to the image, the fallback
          // icon will be a nicer fallback.
          NSString* faviconFileName =
              GetFaviconFileName(net::GURLWithNSURL(siteNSURL));
          NSURL* fileURL =
              [favicons_directory URLByAppendingPathComponent:faviconFileName];
          NSData* imageData = UIImagePNGRepresentation(attributes.faviconImage);

          base::OnceCallback<void()> writeImage = base::BindOnce(^{
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::WILL_BLOCK);
            [imageData writeToURL:fileURL atomically:YES];
          });

          base::ThreadPool::PostTaskAndReply(
              FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
              std::move(writeImage), base::BindOnce(&UpdateShortcutsWidget));
        } else {
          NSDictionary* tiles = ReadSavedMostVisited();
          NTPTile* tile = [tiles objectForKey:siteNSURL];
          if (!tile) {
            return;
          }
          tile.fallbackTextColor = attributes.textColor;
          tile.fallbackBackgroundColor = attributes.backgroundColor;
          tile.fallbackIsDefaultColor = attributes.defaultBackgroundColor;
          tile.fallbackMonogram = attributes.monogramString;
          WriteSingleUpdatedTileToDisk(tile);
          // Favicon is outdated. Delete it.
          NSString* faviconFileName =
              GetFaviconFileName(net::GURLWithNSURL(siteNSURL));
          NSURL* fileURL =
              [favicons_directory URLByAppendingPathComponent:faviconFileName];
          base::OnceCallback<void()> removeImage = base::BindOnce(^{
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::WILL_BLOCK);
            [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
          });

          base::ThreadPool::PostTask(
              FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
              std::move(removeImage));
        }
      };

  [favicon_provider fetchFaviconAttributesForURL:site_url
                                      completion:faviconAttributesBlock];
}
}  // namespace content_suggestions_tile_saver
