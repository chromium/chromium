// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_util.h"

#include <CommonCrypto/CommonDigest.h>
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
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
    if (![[NSFileManager defaultManager]
            fileExistsAtPath:[shared_favicon_attributes_folder_url path]]) {
      [[NSFileManager defaultManager]
                createDirectoryAtPath:[shared_favicon_attributes_folder_url
                                          path]
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
  favicon_loader->FaviconForPageUrl(
      site_url, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        SaveFaviconToSharedAppContainer(attributes, filename);
      });
}
