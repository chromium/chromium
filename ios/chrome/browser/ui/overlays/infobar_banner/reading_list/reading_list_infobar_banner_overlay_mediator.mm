// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/reading_list/reading_list_infobar_banner_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_banner/add_to_reading_list_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using reading_list_infobar_overlay::ReadingListBannerRequestConfig;

namespace {

// The name of the reading list icon image.
NSString* const kReadingListmageName = @"infobar_reading_list";

}  // namespace

@interface AddToReadingListInfobarBannerOverlayMediator ()
// The add to reading list banner config from the request.
@property(nonatomic, readonly) ReadingListBannerRequestConfig* config;
@end

@implementation AddToReadingListInfobarBannerOverlayMediator

#pragma mark - Accessors

- (ReadingListBannerRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<ReadingListBannerRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return ReadingListBannerRequestConfig::RequestSupport();
}

@end

@implementation AddToReadingListInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  ReadingListBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer setTitleText:config->title_text()];
  [self.consumer setSubtitleText:config->message_text()];
  [self.consumer setButtonText:config->button_text()];

  UIImage* iconImage =
      UseSymbols() ? DefaultSymbolTemplateWithPointSize(kReadLaterActionSymbol,
                                                        kInfobarSymbolPointSize)
                   : [UIImage imageNamed:kReadingListmageName];
  [self.consumer setIconImage:iconImage];
  [self.consumer setPresentsModal:YES];
}

@end
