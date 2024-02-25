// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_fetcher_impl.h"
#import "components/image_fetcher/ios/ios_image_decoder_impl.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/gfx/image/image.h"

namespace {

const char kImageFetcherUmaClient[] = "PasswordSharing";

}  // namespace

@interface FamilyPickerMediator () {
  // Contains information about recipients to be displayed in the UI.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;

  // Fetches profile pictures of recipients.
  std::unique_ptr<image_fetcher::ImageFetcher> _imageFetcher;
}

@end

@implementation FamilyPickerMediator

- (instancetype)
        initWithRecipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
    sharedURLLoaderFactory:
        (scoped_refptr<network::SharedURLLoaderFactory>)sharedURLLoaderFactory {
  self = [super init];
  if (self) {
    _recipients = recipients;
    _imageFetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
        image_fetcher::CreateIOSImageDecoder(), sharedURLLoaderFactory);

    [self fetchRecipientIcons];
  }
  return self;
}

- (void)setConsumer:(id<FamilyPickerConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setRecipients:_recipients];
}

#pragma mark - Private

// Fetches recipient icons from their image URLs if they are not already fetched
// and notifies consumer on successful fetch.
- (void)fetchRecipientIcons {
  __weak __typeof__(self) weakSelf = self;
  image_fetcher::ImageFetcherParams params(NO_TRAFFIC_ANNOTATION_YET,
                                           kImageFetcherUmaClient);
  for (RecipientInfoForIOSDisplay* recipient in _recipients) {
    if (recipient.isImageFetched) {
      continue;
    }

    _imageFetcher->FetchImage(
        GURL(base::SysNSStringToUTF8(recipient.profileImageURL)),
        base::BindOnce(^(const gfx::Image& image,
                         const image_fetcher::RequestMetadata& metadata) {
          if (!image.IsEmpty()) {
            [weakSelf updateRecipient:recipient
                         profileImage:[image.ToUIImage() copy]];
          }
        }),
        params);
  }
}

// Updates `recipient` info with their profile `image`.
- (void)updateRecipient:(RecipientInfoForIOSDisplay*)recipient
           profileImage:(UIImage*)image {
  recipient.profileImage = image;
  recipient.imageFetched = YES;
  [_consumer setRecipients:_recipients];
}

@end
