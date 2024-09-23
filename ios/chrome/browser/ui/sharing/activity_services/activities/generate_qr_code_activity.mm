// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/generate_qr_code_activity.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
NSString* const kGenerateQrCodeActivityType =
    @"com.google.chrome.GenerateQrCodeActivityType";
}  // namespace

@interface GenerateQrCodeActivity () {
  GURL _activityURL;
}

@property(nonatomic, weak, readonly) NSString* title;
@property(nonatomic, weak, readonly) id<QRGenerationCommands> handler;

@end

@implementation GenerateQrCodeActivity

- (instancetype)initWithURL:(const GURL&)activityURL
                      title:(NSString*)title
                    handler:(id<QRGenerationCommands>)handler {
  if ((self = [super init])) {
    _activityURL = activityURL;
    _title = title;
    _handler = handler;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kGenerateQrCodeActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_GENERATE_QR_CODE_ACTION);
}

- (UIImage*)activityImage {
  return DefaultSymbolWithPointSize(kQRCodeSymbol, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.handler != nil;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  [self.handler
      generateQRCode:[[GenerateQRCodeCommand alloc] initWithURL:_activityURL
                                                          title:self.title]];
}

@end
