// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/contextual_cue/contextual_cue_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cue_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@interface ContextualCueInfobarBannerOverlayMediator ()
// The default infobar banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation ContextualCueInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the contextual cue delegate attached to the config.
- (contextual_cueing::ContextualCueInfobarDelegate*)contextualCueDelegate {
  return static_cast<contextual_cueing::ContextualCueInfobarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarBannerOverlayMediator

- (void)configureDependenciesWithDispatcher:(CommandDispatcher*)dispatcher {
  [super configureDependenciesWithDispatcher:dispatcher];
  if ([dispatcher dispatchingForProtocol:@protocol(SettingsCommands)]) {
    self.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  }
}

- (void)disconnect {
  self.settingsHandler = nil;
  [super disconnect];
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  contextual_cueing::ContextualCueInfobarDelegate* delegate =
      self.contextualCueDelegate;
  if (!delegate) {
    return;
  }

  if (delegate->Accept()) {
    [self dismissOverlay];
  }
}

#pragma mark - InfobarBannerDelegate

- (void)presentInfobarModalFromBanner {
  id<SettingsCommands> settingsHandler = self.settingsHandler;
  [self dismissOverlay];
  [settingsHandler showGeminiContextualCueSettings];
}

@end

@implementation ContextualCueInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config) {
    return;
  }

  contextual_cueing::ContextualCueInfobarDelegate* delegate =
      self.contextualCueDelegate;
  if (!delegate) {
    return;
  }

  [self.consumer
      setTitleText:base::SysUTF16ToNSString(delegate->GetTitleText())];
  [self.consumer
      setButtonText:base::SysUTF16ToNSString(delegate->GetButtonLabel(
                        ConfirmInfoBarDelegate::BUTTON_OK))];
  if (!delegate->GetIcon().IsEmpty()) {
    [self.consumer setIconImage:delegate->GetIcon().GetImage().ToUIImage()];
    [self.consumer setUseIconBackgroundTint:delegate->UseIconBackgroundTint()];
  }
  [self.consumer setPresentsModal:YES];
}

@end
