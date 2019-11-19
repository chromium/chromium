// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_mediator.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_blocking_action.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_overlay_mediator_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface JavaScriptConfirmationOverlayMediator ()
@property(nonatomic, readonly) OverlayRequest* request;
@property(nonatomic, readonly)
    JavaScriptConfirmationOverlayRequestConfig* config;

// Sets the OverlayResponse using the user's selection from the confirmation UI.
- (void)setConfirmationResponse:(BOOL)dialogConfirmed;
@end

@implementation JavaScriptConfirmationOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
    // Verify that the request is configured for JavaScript confirmations.
    DCHECK(_request->GetConfig<JavaScriptConfirmationOverlayRequestConfig>());
  }
  return self;
}

#pragma mark - Accessors

- (JavaScriptConfirmationOverlayRequestConfig*)config {
  return self.request->GetConfig<JavaScriptConfirmationOverlayRequestConfig>();
}

#pragma mark - Response helpers

// Sets the OverlayResponse using the user's selection from the confirmation UI.
- (void)setConfirmationResponse:(BOOL)dialogConfirmed {
  self.request->set_response(
      OverlayResponse::CreateWithInfo<
          JavaScriptConfirmationOverlayResponseInfo>(dialogConfirmed));
}

@end

@implementation JavaScriptConfirmationOverlayMediator (Subclassing)

- (NSString*)alertTitle {
  return GetJavaScriptDialogTitle(self.config->source(),
                                  self.config->message());
}

- (NSString*)alertMessage {
  return GetJavaScriptDialogMessage(self.config->source(),
                                    self.config->message());
}

- (NSArray<AlertAction*>*)alertActions {
  __weak __typeof__(self) weakSelf = self;
  NSMutableArray* actions = [@[
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf setConfirmationResponse:YES];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf setConfirmationResponse:NO];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
  ] mutableCopy];
  AlertAction* blockingAction =
      GetBlockingAlertAction(self, self.config->source());
  if (blockingAction)
    [actions addObject:blockingAction];
  return actions;
}

@end
