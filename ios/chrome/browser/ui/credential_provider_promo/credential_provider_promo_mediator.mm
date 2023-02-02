// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"

#import "base/files/file_path.h"
#import "base/path_service.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_constants.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kFirstStepAnimation = @"CPE_promo_animation_edu";
}  // namespace

@interface CredentialProviderPromoMediator ()

// The main consumer for this mediator.
@property(nonatomic, weak) id<CredentialProviderPromoConsumer> consumer;

// The PrefService used by this mediator.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation CredentialProviderPromoMediator

- (instancetype)initWithConsumer:(id<CredentialProviderPromoConsumer>)consumer
                     prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _prefService = prefService;
  }
  return self;
}

- (BOOL)canShowCredentialProviderPromo {
  // TODO(crbug.com/1392116): check for user action and impression counts
  return IsCredentialProviderExtensionPromoEnabled() &&
         !password_manager_util::IsCredentialProviderEnabledOnStartup(
             self.prefService);
}

- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger {
  CredentialProviderPromoSource source;
  CredentialProviderPromoContext context =
      CredentialProviderPromoContext::kFirstStep;
  switch (trigger) {
    case CredentialProviderPromoTrigger::PasswordCopied:
      source = CredentialProviderPromoSource::kPasswordCopied;
      [self setAnimationWithContext:context];
      break;
    case CredentialProviderPromoTrigger::PasswordSaved:
      source = CredentialProviderPromoSource::kPasswordSaved;
      break;
    case CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword:
      source = CredentialProviderPromoSource::kAutofillUsed;
      break;
    case CredentialProviderPromoTrigger::RemindMeLater:
      source = CredentialProviderPromoSource::kRemindLaterSelected;
      break;
  }

  [self setTextAndImageWithContext:context source:source];
}

#pragma mark - Private

// Sets the animation to the consumer that corresponds to the value of
// `context`. Depending on the value of `context`, either the 'first step' or
// 'learn more' animation path is set.
- (void)setAnimationWithContext:(CredentialProviderPromoContext)context {
  NSString* animationName;
  if (context == CredentialProviderPromoContext::kFirstStep) {
    animationName = kFirstStepAnimation;
  } else {
    // TODO(crbug.com/1392116): set animation for
    // CredentialProviderPromoContext::kLearnMore.
    animationName = kFirstStepAnimation;
  }
  NSString* path = [[NSBundle mainBundle] pathForResource:animationName
                                                   ofType:@"json"];
  [self.consumer setAnimation:path];
}

// Sets the text and image to the consumer. The text set depends on the value of
// `context`. When `source` is kPasswordCopied, no image is set.
- (void)setTextAndImageWithContext:(CredentialProviderPromoContext)context
                            source:(CredentialProviderPromoSource)source {
  if (context == CredentialProviderPromoContext::kFirstStep) {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_TITLE);
    NSString* subtitleString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_SUBTITLE);
    NSString* primaryActionString =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_HOW);
    NSString* secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS);
    NSString* tertiaryActionString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_REMIND_ME_LATER);
    UIImage* image = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kPasswordSuggestionKey);
    if (source == CredentialProviderPromoSource::kPasswordCopied) {
      image = nil;
    }
    [self.consumer setTitleString:titleString
                   subtitleString:subtitleString
              primaryActionString:primaryActionString
            secondaryActionString:secondaryActionString
             tertiaryActionString:tertiaryActionString
                            image:image];
  } else {
    // TODO(crbug.com/1392116): configure for
    // CredentialProviderPromoContext::kLearnMore.
  }
}

@end
