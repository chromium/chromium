// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/tab_pickup/tab_pickup_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/tab_pickup_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"

@interface TabPickupBannerOverlayMediator ()

// The tab pickup banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation TabPickupBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (TabPickupInfobarDelegate*)tabPickupDelegate {
  return static_cast<TabPickupInfobarDelegate*>(self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // This can happen if the user quickly navigates to another website while the
  // banner is still appearing, causing the banner to be triggered before being
  // removed.
  if (!self.tabPickupDelegate) {
    return;
  }

  self.tabPickupDelegate->OpenDistantTab();

  [self dismissOverlay];
}

@end

@implementation TabPickupBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  TabPickupInfobarDelegate* delegate = self.tabPickupDelegate;

  NSString* title =
      l10n_util::GetNSStringF(IDS_IOS_TAB_PICKUP_BANNER_TITLE,
                              base::UTF8ToUTF16(delegate->GetSessionName()));

  const GURL& tabURL = delegate->GetTabURL();
  NSString* hostname = [self hostnameFromGURL:tabURL];
  NSString* subtitle = [NSString
      stringWithFormat:@"%@ • %@", hostname,
                       [self lastSyncTimeStringFromTime:delegate
                                                            ->GetSyncedTime()]];

  UIImage* faviconImage = delegate->GetFaviconImage();
  if (faviconImage) {
    [self.consumer setFaviconImage:faviconImage];
  } else {
    [self.consumer setIconImage:CustomSymbolWithPointSize(
                                    kRecentTabsSymbol, kInfobarBannerIconSize)];
  }

  [self.consumer setTitleText:title];
  [self.consumer setTitleNumberOfLines:2];
  [self.consumer setSubtitleText:subtitle];
  [self.consumer setSubtitleNumberOfLines:2];
  [self.consumer setSubtitleLineBreakMode:NSLineBreakByTruncatingHead];
  [self.consumer
      setButtonText:l10n_util::GetNSString(IDS_IOS_TAB_PICKUP_BANNER_BUTTON)];
  [self.consumer setFaviconImage:faviconImage];
  [self.consumer setPresentsModal:YES];
}

#pragma mark - Private

// Returns the tab hostname from the given `URL`.
- (NSString*)hostnameFromGURL:(GURL)URL {
  return base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              URL));
}

// Returns the last sync string from the given `time`.
- (NSString*)lastSyncTimeStringFromTime:(base::Time)time {
  NSString* timeString;
  base::TimeDelta lastUsedDelta = base::Time::Now() - time;
  base::TimeDelta oneMinuteDelta = base::Minutes(1);

  // If the tab was synchronized within the last minute, set the time delta to 1
  // minute.
  if (lastUsedDelta < oneMinuteDelta) {
    lastUsedDelta = oneMinuteDelta;
  }

  NSDate* date = [NSDate dateWithTimeIntervalSince1970:time.ToTimeT()];
  timeString =
      [NSDateFormatter localizedStringFromDate:date
                                     dateStyle:NSDateFormatterNoStyle
                                     timeStyle:NSDateFormatterShortStyle];

  timeString = base::SysUTF16ToNSString(
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                             ui::TimeFormat::LENGTH_SHORT, lastUsedDelta));
  // This will return something similar to "1 min/hour ago".
  return [NSString stringWithFormat:@"%@", timeString];
}

@end
