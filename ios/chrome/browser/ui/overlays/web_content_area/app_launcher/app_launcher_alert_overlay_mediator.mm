// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/app_launcher/app_launcher_alert_overlay_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/web_content_area/app_launcher_alert_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator+subclassing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppLauncherAlertOverlayMediator ()
@property(nonatomic, readonly) OverlayRequest* request;
@property(nonatomic, readonly) AppLauncherAlertOverlayRequestConfig* config;

// Sets the OverlayResponse. |allowAppLaunch| indicates whether the alert's
// allow button was tapped to allow the navigation to open in another app.
- (void)updateResponseAllowingAppLaunch:(BOOL)allowAppLaunch;
@end

@implementation AppLauncherAlertOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
    // Verify that the request is configured for app launcher alerts.
    DCHECK(_request->GetConfig<AppLauncherAlertOverlayRequestConfig>());
  }
  return self;
}

#pragma mark - Accessors

- (AppLauncherAlertOverlayRequestConfig*)config {
  return self.request->GetConfig<AppLauncherAlertOverlayRequestConfig>();
}

#pragma mark - Response helpers

- (void)updateResponseAllowingAppLaunch:(BOOL)allowAppLaunch {
  self.request->set_response(
      OverlayResponse::CreateWithInfo<AppLauncherAlertOverlayResponseInfo>(
          allowAppLaunch));
}

@end

@implementation AppLauncherAlertOverlayMediator (Subclassing)

- (NSString*)alertMessage {
  return self.config->is_repeated_request()
             ? l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP)
             : l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP);
}

- (NSArray<AlertAction*>*)alertActions {
  NSString* rejectActionTitle = l10n_util::GetNSString(IDS_CANCEL);
  NSString* allowActionTitle =
      self.config->is_repeated_request()
          ? l10n_util::GetNSString(IDS_IOS_OPEN_REPEATEDLY_ANOTHER_APP_ALLOW)
          : l10n_util::GetNSString(IDS_IOS_APP_LAUNCHER_OPEN_APP_BUTTON_LABEL);
  __weak __typeof__(self) weakSelf = self;
  return @[
    [AlertAction actionWithTitle:allowActionTitle
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseAllowingAppLaunch:YES];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:rejectActionTitle
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseAllowingAppLaunch:NO];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
  ];
}

@end
