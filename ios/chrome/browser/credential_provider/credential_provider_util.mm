// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_util.h"

#include <CommonCrypto/CommonDigest.h>
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_store.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::SysUTF16ToNSString;
using base::UTF8ToUTF16;

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
  return base::SysUTF8ToNSString(
      base::HexEncode(result, CC_SHA256_DIGEST_LENGTH));
}

void SaveFaviconToSharedAppContainer(FaviconAttributes* attributes,
                                     NSString* filename) {
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:attributes
                                       requiringSecureCoding:NO
                                                       error:&error];
  if (!data || error) {
    DLOG(WARNING) << base::SysNSStringToUTF8([error description]);
    return;
  }

  base::OnceCallback<void()> write_image = base::BindOnce(^{
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
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(write_image));
}

void FetchFaviconForURLToPath(FaviconLoader* favicon_loader,
                              const GURL& site_url,
                              NSString* filename) {
  DCHECK(favicon_loader);
  DCHECK(filename);
  favicon_loader->FaviconForPageUrl(
      site_url, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        SaveFaviconToSharedAppContainer(attributes, filename);
      });
}

// Clean up obsolete favicons from the Chrome app group storage.
void CleanUpFavicons() {
  // TODO(crbug.com/1300569): Remove this when kEnableFaviconForPasswords flag
  // is removed.
  NSFileManager* file_manager = [NSFileManager defaultManager];
  NSString* path = app_group::SharedFaviconAttributesFolder().path;
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnableFaviconForPasswords)) {
    // Remove repo.
    if ([file_manager fileExistsAtPath:path]) {
      [file_manager removeItemAtPath:path error:nil];
    }
    return;
  }

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
    if (credential.favicon) {
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
}

void CleanUpFaviconsInBackground() {
  base::OnceCallback<void()> favicon_cleanup = base::BindOnce(^{
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    CleanUpFavicons();
  });
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      std::move(favicon_cleanup));
}

void UpdateFaviconsStorage(FaviconLoader* favicon_loader) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnableFaviconForPasswords)) {
    ArchivableCredentialStore* archivable_store =
        [[ArchivableCredentialStore alloc]
            initWithFileURL:CredentialProviderSharedArchivableStoreURL()];
    NSArray<id<Credential>>* all_credentials = archivable_store.credentials;

    for (id<Credential> credential : all_credentials) {
      GURL url = GURL(base::SysNSStringToUTF8(credential.serviceIdentifier));
      NSString* filename = credential.favicon;
      if (!credential.favicon) {
        // Add favicon name to the credential and update the store.
        filename = GetFaviconFileKey(url);
        ArchivableCredential* newCredential = [[ArchivableCredential alloc]
                 initWithFavicon:filename
              keychainIdentifier:credential.keychainIdentifier
                            rank:credential.rank
                recordIdentifier:credential.recordIdentifier
               serviceIdentifier:credential.serviceIdentifier
                     serviceName:credential.serviceName
                            user:credential.user
            validationIdentifier:credential.validationIdentifier];
        if ([archivable_store
                credentialWithRecordIdentifier:newCredential
                                                   .recordIdentifier]) {
          [archivable_store updateCredential:newCredential];
        } else {
          [archivable_store addCredential:newCredential];
        }
      }

      // Fetch the favicon and save it to the app group storage.
      if (filename) {
        FetchFaviconForURLToPath(favicon_loader, url, filename);
      }
    }

    // Save changes in the credential store and call the clean up method to
    // remove obsolete favicons from the Chrome app group storage.
    [archivable_store saveDataWithCompletion:^(NSError* error) {
      CleanUpFaviconsInBackground();
    }];
  } else {
    // Call clean up to remove the repo when the flag is off.
    CleanUpFaviconsInBackground();
  }
}
