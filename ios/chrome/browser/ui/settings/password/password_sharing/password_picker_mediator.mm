// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_mediator.h"

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

@interface PasswordPickerMediator () {
  // Information about credentials for affiliated group from which this password
  // sharing flow originated.
  std::vector<password_manager::CredentialUIEntry> _credentials;

  // Used to fetch favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;
}

@end

@implementation PasswordPickerMediator

- (instancetype)initWithCredentials:
                    (const std::vector<password_manager::CredentialUIEntry>&)
                        credentials
                      faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    _credentials = credentials;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)setConsumer:(id<PasswordPickerConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setCredentials:_credentials];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

@end
