// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_saver.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/ntp_tiles/ntp_tile.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "crypto/obsolete/md5.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ntp_tile/ntp_tile.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "net/base/apple/url_conversions.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/model/model_swift.h"
#endif

namespace content_suggestions_tile_saver {

// Writes the `most_visited_sites` to disk.
void WriteSavedMostVisited(
    NSDictionary<NSURL*, NTPTile*>* most_visited_sites,
    ChromeAccountManagerService* account_manager_service);

// Checks if every site in `tiles` has had its favicons fetched. If so, writes
// the info to disk, saving the favicons to `favicons_directory`.
void WriteToDiskIfComplete(NSDictionary<NSURL*, NTPTile*>* tiles,
                           NSURL* favicons_directory);

// Gets a name for the favicon file.
NSString* GetFaviconFileName(const GURL& url);

// Decodes data.
NSDictionary* DecodeData(NSData* data);

// If the sites currently saved include one with `tile`'s url, replace it by
// `tile`.
void WriteSingleUpdatedTileToDisk(NTPTile* tile);

// Gets the favicons using `favicon_provider` and writes them to disk.
void GetFaviconsAndSave(
    const ntp_tiles::NTPTilesVector& most_visited_data,
    __strong FaviconAttributesProvider* favicon_provider,
    __strong NSURL* favicons_directory,
    base::WeakPtr<ChromeAccountManagerService> weak_account_manager_service);

// Updates the list of tiles that must be displayed in the content suggestion
// widget.
void UpdateTileList(const ntp_tiles::NTPTilesVector& most_visited_data,
                    ChromeAccountManagerService* account_manager_service);

// Deletes icons contained in `favicons_directory` and corresponding to no URL
// in `most_visited_data`.
void ClearOutdatedIcons(const ntp_tiles::NTPTilesVector& most_visited_data,
                        NSURL* favicons_directory);

}  // namespace content_suggestions_tile_saver

namespace content_suggestions_tile_saver {

void UpdateTileList(const ntp_tiles::NTPTilesVector& most_visited_data,
                    ChromeAccountManagerService* account_manager_service) {
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
  WriteSavedMostVisited(tiles, account_manager_service);
}

std::string Md5AsHexForFaviconUrl(std::string_view url) {
  return base::HexEncodeLower(crypto::obsolete::Md5::Hash(url));
}

NSString* GetFaviconFileName(const GURL& url) {
  return [base::SysUTF8ToNSString(Md5AsHexForFaviconUrl(url.spec()))
      stringByAppendingString:@".png"];
}

NSDictionary* DecodeData(NSData* data) {
  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
  if (!unarchiver || error) {
    DLOG(WARNING) << "Error creating unarchiver for most visited: "
                  << base::SysNSStringToUTF8([error description]);
    return [[NSMutableDictionary alloc] init];
  }

  unarchiver.requiresSecureCoding = NO;
  return [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
}

void GetFaviconsAndSave(
    const ntp_tiles::NTPTilesVector& most_visited_data,
    __strong FaviconAttributesProvider* favicon_provider,
    __strong NSURL* favicons_directory,
    base::WeakPtr<ChromeAccountManagerService> weak_account_manager_service) {
  ChromeAccountManagerService* account_manager_service =
      weak_account_manager_service.get();
  if (!account_manager_service) {
    return;
  }

  for (size_t i = 0; i < most_visited_data.size(); i++) {
    const GURL& gurl = most_visited_data[i].url;
    UpdateSingleFavicon(gurl, favicon_provider, favicons_directory,
                        account_manager_service);
  }
}

void ClearOutdatedIcons(const ntp_tiles::NTPTilesVector& most_visited_data,
                        NSURL* favicons_directory) {
  NSMutableSet<NSString*>* allowed_files_name = [[NSMutableSet alloc] init];

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
  // Add in `allowed_files_name` information about all profiles.
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSDictionary* suggested_items =
      [shared_defaults objectForKey:app_group::kSuggestedItemsForMultiprofile];
  NSArray<NSData*>* all_data = [suggested_items allValues];
  for (NSData* data_for_account in all_data) {
    NSArray<NTPTile*>* tiles = [DecodeData(data_for_account) allValues];
    // Add urls to the set of allowed_files_name.
    for (NTPTile* tile in tiles) {
      [allowed_files_name addObject:tile.faviconFileName];
    }
  }
#else
  for (size_t i = 0; i < most_visited_data.size(); i++) {
    const ntp_tiles::NTPTile& ntp_tile = most_visited_data[i];
    NSString* favicon_file_name = GetFaviconFileName(ntp_tile.url);
    [allowed_files_name addObject:favicon_file_name];
  }
#endif

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

void SaveMostVisitedToDisk(
    const ntp_tiles::NTPTilesVector& most_visited_data,
    __strong FaviconAttributesProvider* favicon_provider,
    __strong NSURL* favicons_directory,
    ChromeAccountManagerService* account_manager_service) {
  if (favicons_directory == nil) {
    return;
  }
  UpdateTileList(most_visited_data, account_manager_service);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ClearOutdatedIcons, most_visited_data,
                     favicons_directory),
      base::BindOnce(&GetFaviconsAndSave, most_visited_data, favicon_provider,
                     favicons_directory,
                     account_manager_service->GetWeakPtr()));
}

void WriteSingleUpdatedTileToDisk(
    NTPTile* tile,
    ChromeAccountManagerService* account_manager_service) {
  NSMutableDictionary* tiles = [ReadSavedMostVisited() mutableCopy];
  if (![tiles objectForKey:tile.URL]) {
    return;
  }
  [tiles setObject:tile forKey:tile.URL];
  WriteSavedMostVisited(tiles, account_manager_service);
}

// Updates the Shortcut's widget with the user's current most visited sites
void UpdateShortcutsWidget() {
#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
  [WidgetTimelinesUpdater reloadTimelinesOfKind:@"ShortcutsWidget"];
#endif
}

void WriteSavedMostVisited(
    NSDictionary<NSURL*, NTPTile*>* most_visited_data,
    ChromeAccountManagerService* account_manager_service) {
  DCHECK(account_manager_service);

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

  // TODO(crbug.com/387971524): To be removed once
  // ios_enable_widgets_for_mim is enabled by default.
  [sharedDefaults setObject:data forKey:app_group::kSuggestedItems];
  [sharedDefaults setObject:last_modification_date
                     forKey:app_group::kSuggestedItemsLastModificationDate];

  NSMutableDictionary* suggested_items = [[sharedDefaults
      objectForKey:app_group::kSuggestedItemsForMultiprofile] mutableCopy];
  if (suggested_items == nil) {
    suggested_items = [NSMutableDictionary dictionary];
  }

  NSMutableDictionary* last_modification_dates = [[sharedDefaults
      objectForKey:app_group::
                       kSuggestedItemsLastModificationDateForMultiprofile]
      mutableCopy];
  if (last_modification_dates == nil) {
    last_modification_dates = [NSMutableDictionary dictionary];
  }

  std::string profileName = account_manager_service->GetProfileName();
  std::string personalProfileName = GetApplicationContext()
                                        ->GetAccountProfileMapper()
                                        ->GetPersonalProfileName();

  if (profileName == personalProfileName) {
    // If we are in personal profile, data is saved also to "No account". This
    // will be used to retrieve data for a widget with no signed-in account.
    [suggested_items setObject:data forKey:app_group::kNoAccount];
    [last_modification_dates setObject:last_modification_date
                                forKey:app_group::kNoAccount];
  }

  // Always update last modification date for "Default" scenario.
  [suggested_items setObject:data forKey:app_group::kDefault];
  [last_modification_dates setObject:last_modification_date
                              forKey:app_group::kDefault];

  // Update stored info for all identities in the current profile.
  for (id<SystemIdentity> identity in account_manager_service
           ->GetAllIdentities()) {
    NSString* gaia_id_string = identity.gaiaId.ToNSString();
    [suggested_items setObject:data forKey:gaia_id_string];
    [last_modification_dates setObject:last_modification_date
                                forKey:gaia_id_string];
  }

  // Update NSUserDefaults keys.
  [sharedDefaults setObject:suggested_items
                     forKey:app_group::kSuggestedItemsForMultiprofile];
  [sharedDefaults
      setObject:last_modification_dates
         forKey:app_group::kSuggestedItemsLastModificationDateForMultiprofile];

  UpdateShortcutsWidget();
}

NSDictionary* ReadSavedMostVisited() {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSDictionary* data =
      DecodeData([sharedDefaults objectForKey:app_group::kSuggestedItems]);
  return data;
}

void UpdateSingleFavicon(const GURL& site_url,
                         FaviconAttributesProvider* favicon_provider,
                         NSURL* favicons_directory,
                         ChromeAccountManagerService* account_manager_service) {
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
          WriteSingleUpdatedTileToDisk(tile, account_manager_service);
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
