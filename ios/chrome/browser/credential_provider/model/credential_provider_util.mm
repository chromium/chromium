// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_util.h"

#import <CommonCrypto/CommonDigest.h>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "net/base/apple/url_conversions.h"

using base::SysUTF16ToNSString;
using base::UTF8ToUTF16;

extern const char kSyncStoreHistogramName[] =
    "IOS.CredentialExtension.SyncedStore";

namespace {

// Max number of stored favicons.
int const kMaxNumberOfFavicons = 2000;

// Key used to store the favicons last sync date in the user preferences.
NSString* const kFaviconsLastSyncDatePrefKey = @"FaviconsLastSyncDatePrefKey";

// The minimum number of days between the last sync and today.
constexpr base::TimeDelta kResyncInterval = base::Days(7);

// The number of days after which to refetch an existing favicon. This number
// aims at balancing not fetching too often while ensuring the users see updated
// favicons relatively quickly.
constexpr base::TimeDelta kFaviconRefreshInterval = base::Days(14);

}  // namespace

NSString* RecordIdentifierForPasswordForm(
    const password_manager::PasswordForm& form) {
  // These are the UNIQUE keys in the login database.
  return SysUTF16ToNSString(UTF8ToUTF16(form.url.spec() + "|") +
                            form.username_element + u"|" + form.username_value +
                            u"|" + form.password_element +
                            UTF8ToUTF16("|" + form.signon_realm));
}

NSString* GetFaviconFileKey(const GURL& url) {
  // We are using SHA256 hashing to hide the website's URL and also to be able
  // to use as the key for the storage (since the character string that makes up
  // a URL (including the scheme and ://) isn't a valid file name).
  unsigned char result[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(url.spec().data(), url.spec().length(), result);
  return base::SysUTF8ToNSString(base::HexEncode(result));
}

void SaveFaviconToSharedAppContainer(FaviconAttributes* attributes,
                                     NSString* filename) {
  base::OnceCallback<void()> write_image = base::BindOnce(^{
    NSError* error = nil;
    NSData* data = [NSKeyedArchiver archivedDataWithRootObject:attributes
                                         requiringSecureCoding:NO
                                                         error:&error];
    if (!data || error) {
      DLOG(WARNING) << base::SysNSStringToUTF8([error description]);
      return;
    }

    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    NSURL* shared_favicon_attributes_folder_url =
        app_group::SharedFaviconAttributesFolder();
    NSURL* file_url = [shared_favicon_attributes_folder_url
        URLByAppendingPathComponent:filename
                        isDirectory:NO];

    // Create shared folder if it doesn't exist.
    NSFileManager* file_manager = [NSFileManager defaultManager];
    NSString* path = shared_favicon_attributes_folder_url.path;
    if (![file_manager fileExistsAtPath:path]) {
      [file_manager createDirectoryAtPath:path
              withIntermediateDirectories:YES
                               attributes:nil
                                    error:nil];
    }

    // Write to file.
    [data writeToURL:file_url atomically:YES];
  });
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::ThreadPolicy::PREFER_BACKGROUND,
       base::TaskPriority::BEST_EFFORT},
      std::move(write_image));
}

// Returns true to continue fetching favicon if the app group storage does not
// contain more than the max number of favicons or if the verification is
// skipped.
bool ShouldContinueFetchingFavicon() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  return
      [[[NSFileManager defaultManager]
          contentsOfDirectoryAtPath:app_group::SharedFaviconAttributesFolder()
                                        .path
                              error:nil] count] < kMaxNumberOfFavicons;
}

// Fetches favicon from site URL and saves it to `filename`.
void ContinueFetchingFavicon(base::WeakPtr<FaviconLoader> weak_favicon_loader,
                             const GURL& site_url,
                             NSString* filename,
                             bool fallback_to_google_server,
                             bool continue_fetching) {
  FaviconLoader* favicon_loader = weak_favicon_loader.get();
  if (!continue_fetching || !favicon_loader) {
    // Reached max number of stored favicons or favicon loader is null.
    return;
  }
  // Fallback to Google server for synced user only.
  favicon_loader->FaviconForPageUrl(
      site_url, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      fallback_to_google_server, ^(FaviconAttributes* attributes) {
        SaveFaviconToSharedAppContainer(attributes, filename);
      });
}

void FetchFaviconForURLToPath(FaviconLoader* favicon_loader,
                              const GURL& site_url,
                              NSString* filename,
                              bool skip_max_verification,
                              bool fallback_to_google_server) {
  DCHECK(favicon_loader);
  DCHECK(filename);
  if (skip_max_verification) {
    ContinueFetchingFavicon(favicon_loader->AsWeakPtr(), site_url, filename,
                            fallback_to_google_server,
                            /* continue_fetching */ YES);
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::ThreadPolicy::PREFER_BACKGROUND,
         base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&ShouldContinueFetchingFavicon),
        base::BindOnce(&ContinueFetchingFavicon, favicon_loader->AsWeakPtr(),
                       site_url, filename, fallback_to_google_server));
  }
}

// Gets the last sync date for favicons in the app group storage.
base::Time GetFaviconsLastSyncDate() {
  NSDate* last_sync_date =
      base::apple::ObjCCast<NSDate>([[NSUserDefaults standardUserDefaults]
          objectForKey:kFaviconsLastSyncDatePrefKey]);
  // If no value stored in the NSUserDefaults, consider that the last sync
  // happened forever ago.
  if (!last_sync_date) {
    return base::Time();
  }
  return base::Time::FromNSDate(last_sync_date);
}

// Sets the value of the last sync date for favicons in the app group storage.
void SetFaviconsLastSyncDate(base::Time sync_time) {
  [[NSUserDefaults standardUserDefaults]
      setObject:sync_time.ToNSDate()
         forKey:kFaviconsLastSyncDatePrefKey];
}

// Cleans up obsolete favicons from the Chrome app group storage.
void CleanUpFavicons(NSSet* excess_favicons_filenames) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* path = app_group::SharedFaviconAttributesFolder().path;
  NSArray* filename_list = [file_manager contentsOfDirectoryAtPath:path
                                                             error:nil];
  if (!filename_list || [filename_list count] == 0) {
    return;
  }

  ArchivableCredentialStore* archivable_store =
      [[ArchivableCredentialStore alloc]
          initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  NSArray<id<Credential>>* all_credentials = archivable_store.credentials;
  // Extract favicon filename from the credentials list.
  NSMutableSet* credential_favicon_filename_set =
      [[NSMutableSet alloc] initWithCapacity:all_credentials.count];
  for (id<Credential> credential in all_credentials) {
    if (credential.favicon &&
        ![excess_favicons_filenames containsObject:credential.favicon]) {
      [credential_favicon_filename_set addObject:credential.favicon];
    }
  }

  for (NSUInteger i = 0; i < [filename_list count]; i++) {
    NSString* filename = [filename_list objectAtIndex:i];
    if (![credential_favicon_filename_set containsObject:filename]) {
      // Remove from storage.
      NSURL* url = [app_group::SharedFaviconAttributesFolder()
          URLByAppendingPathComponent:filename
                          isDirectory:NO];
      if ([file_manager fileExistsAtPath:[url path]]) {
        [file_manager removeItemAtURL:url error:nil];
      }
    }
  }

  // Store the last sync date.
  SetFaviconsLastSyncDate(base::Time::Now());
}

void UpdateFaviconsStorage(FaviconLoader* favicon_loader,
                           bool fallback_to_google_server) {
  // Verify if the app group storage for favicons needs to be synced and
  // cleaned up by checking the last sync date.
  const base::TimeDelta time_elapsed_since_last_sync =
      base::Time::Now() - GetFaviconsLastSyncDate();
  if (time_elapsed_since_last_sync < kResyncInterval) {
    return;
  }
  ArchivableCredentialStore* archivable_store =
      [[ArchivableCredentialStore alloc]
          initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
  NSArray<id<Credential>>* all_credentials = archivable_store.credentials;

  // Sort by highest rank.
  NSArray<id<Credential>>* all_credentials_rank =
      [all_credentials sortedArrayUsingComparator:^NSComparisonResult(
                           id<Credential> c1, id<Credential> c2) {
        if (c1.rank == c2.rank) {
          return NSOrderedSame;
        }
        return c1.rank > c2.rank ? NSOrderedAscending : NSOrderedDescending;
      }];

  // Truncate array if it is larger than the maximum and add the favicons
  // file name to be removed in a set.
  NSMutableSet* excess_favicons_filenames = [[NSMutableSet alloc] init];
  if ([all_credentials_rank count] > kMaxNumberOfFavicons) {
    for (NSUInteger i = kMaxNumberOfFavicons; i < all_credentials_rank.count;
         i++) {
      if ([all_credentials_rank objectAtIndex:i].favicon) {
        [excess_favicons_filenames
            addObject:[all_credentials_rank objectAtIndex:i].favicon];
      }
    }
    all_credentials_rank = [all_credentials_rank
        subarrayWithRange:NSMakeRange(0, kMaxNumberOfFavicons)];
  }

  for (id<Credential> credential : all_credentials_rank) {
    GURL url = GURL(base::SysNSStringToUTF8(credential.serviceIdentifier));
    if (!url.is_valid()) {
      // Skip fetching the favicon for credential with invalid URL.
      continue;
    }
    NSString* filename = credential.favicon;
    if (!credential.favicon) {
      // Add favicon name to the credential and update the store.
      filename = GetFaviconFileKey(url);
      ArchivableCredential* newCredential =
          [[ArchivableCredential alloc] initWithFavicon:filename
                                             credential:credential];
      if ([archivable_store
              credentialWithRecordIdentifier:newCredential.recordIdentifier]) {
        [archivable_store updateCredential:newCredential];
      } else {
        [archivable_store addCredential:newCredential];
      }
    }

    // Fetch the favicon and save it to the app group storage.
    if (filename) {
      FetchFaviconForURLToPath(favicon_loader, url, filename,
                               /*skip_max_verification=*/YES,
                               fallback_to_google_server);

      // Remove file name duplicate because it is part of the top
      // `kMaxNumberOfFavicons` credentials used by the user.
      if ([excess_favicons_filenames containsObject:filename]) {
        [excess_favicons_filenames removeObject:filename];
      }
    }
  }

  // Save changes in the credential store and call the clean up method to
  // remove obsolete favicons from the Chrome app group storage.
  [archivable_store saveDataWithCompletion:^(NSError* error) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&CleanUpFavicons, excess_favicons_filenames));
  }];
}

void UpdateFaviconsStorageForBrowserState(
    base::WeakPtr<ProfileIOS> weak_profile,
    bool fallback_to_google_server) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }
  UpdateFaviconsStorage(IOSChromeFaviconLoaderFactory::GetForProfile(profile),
                        fallback_to_google_server);
}

NSDictionary<NSString*, NSDate*>* GetFaviconsListAndFreshness() {
  NSURL* shared_favicon_attributes_folder_url =
      app_group::SharedFaviconAttributesFolder();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* path = shared_favicon_attributes_folder_url.path;

  // If the favicon folder doesn't exist, there are not favicons stored.
  if (![file_manager fileExistsAtPath:path]) {
    return nil;
  }

  NSArray<NSString*>* fileNames = [file_manager contentsOfDirectoryAtPath:path
                                                                    error:nil];
  if (fileNames.count == 0) {
    return nil;
  }

  NSMutableDictionary<NSString*, NSDate*>* favicon_info_dict =
      [[NSMutableDictionary alloc] init];
  for (NSString* fileName in fileNames) {
    NSURL* filePath = [shared_favicon_attributes_folder_url
        URLByAppendingPathComponent:fileName
                        isDirectory:NO];
    NSDictionary* fileAttribs =
        [file_manager attributesOfItemAtPath:filePath.path error:nil];
    if (fileAttribs) {
      [favicon_info_dict setObject:fileAttribs[NSFileCreationDate]
                            forKey:fileName];
    }
  }
  return favicon_info_dict;
}

bool ShouldFetchFavicon(NSString* favicon_key,
                        NSDictionary<NSString*, NSDate*>* favicon_dict) {
  if (!favicon_dict) {
    return true;
  }

  // If there is not previous fetch date, it means there is no favicon for that
  // key. Fetch it.
  NSDate* favicon_fetch_date = [favicon_dict valueForKey:favicon_key];
  if (!favicon_fetch_date) {
    return true;
  }

  // Re-fetch the favicon if it's older than the threshold.
  return base::Time::Now() - base::Time::FromNSDate(favicon_fetch_date) >
         kFaviconRefreshInterval;
}

bool DeleteFaviconsFolder() {
  NSURL* shared_favicon_attributes_folder_url =
      app_group::SharedFaviconAttributesFolder();

  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* path = shared_favicon_attributes_folder_url.path;

  // If the favicon folder doesn't exist, there's nothing to delete.
  if (![file_manager fileExistsAtPath:path]) {
    return true;
  }

  return [file_manager removeItemAtPath:path error:nil];
}
