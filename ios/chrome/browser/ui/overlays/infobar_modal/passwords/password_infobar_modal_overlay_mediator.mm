// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_request_support.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_infobar_modal_responses::UpdateCredentialsInfo;
using password_infobar_modal_responses::NeverSaveCredentials;
using password_infobar_modal_responses::PresentPasswordSettings;

@interface PasswordInfobarModalOverlayMediator ()
// The save password modal config from the request.
@property(nonatomic, readonly) PasswordInfobarModalOverlayRequestConfig* config;
@end

@implementation PasswordInfobarModalOverlayMediator

#pragma mark - Accessors

- (PasswordInfobarModalOverlayRequestConfig*)config {
  return self.request
             ? self.request
                   ->GetConfig<PasswordInfobarModalOverlayRequestConfig>()
             : nullptr;
}

- (void)setConsumer:(id<InfobarPasswordModalConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;

  PasswordInfobarModalOverlayRequestConfig* config = self.config;
  if (!_consumer || !config)
    return;

  [_consumer setUsername:config->username()];
  NSString* password = config->password();
  [_consumer setMaskedPassword:[@"" stringByPaddingToLength:password.length
                                                 withString:@"•"
                                            startingAtIndex:0]];
  [_consumer setUnmaskedPassword:password];
  [_consumer setDetailsTextMessage:config->details_text()];
  [_consumer setSaveButtonText:config->save_button_text()];
  [_consumer setCancelButtonText:config->cancel_button_text()];
  [_consumer setURL:config->url()];
  [_consumer setCurrentCredentialsSaved:config->is_current_password_saved()];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return PasswordInfobarModalOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarPasswordModalDelegate

- (void)updateCredentialsWithUsername:(NSString*)username
                             password:(NSString*)password {
  // Receiving this delegate callback when the request is null means that the
  // update credentials button was tapped after the request was cancelled, but
  // before the modal UI has finished being dismissed.
  if (!self.request)
    return;

  [self dispatchResponse:OverlayResponse::CreateWithInfo<UpdateCredentialsInfo>(
                             username, password)];
  [self modalInfobarButtonWasAccepted:nil];
}

- (void)neverSaveCredentialsForCurrentSite {
  // Receiving this delegate callback when the request is null means that the
  // never save credentials button was tapped after the request was cancelled,
  // but before the modal UI has finished being dismissed.
  if (!self.request)
    return;

  [self
      dispatchResponse:OverlayResponse::CreateWithInfo<NeverSaveCredentials>()];
  [self dismissInfobarModal:nil];
}

- (void)presentPasswordSettings {
  // Receiving this delegate callback when the request is null means that the
  // present passwords settings button was tapped after the request was
  // cancelled, but before the modal UI has finished being dismissed.
  if (!self.request)
    return;

  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             PresentPasswordSettings>()];
  [self dismissInfobarModal:nil];
}

@end
