// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"

#import "base/files/file_path.h"
#import "base/path_service.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_consumer.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kFirstStepAnimation = @"CPE_promo_animation_edu_autofill";
NSString* const kLearnMoreAnimation = @"CPE_promo_animation_edu_how_to_enable";
}  // namespace

@interface CredentialProviderPromoMediator ()

// The PrefService used by this mediator.
@property(nonatomic, assign) PrefService* prefService;

// Local state is used to check promo trigger requirements.
@property(nonatomic, assign) PrefService* localState;

// Indicates whether the 'first step' or 'learn more' version of the promo is
// being presented.
@property(nonatomic, assign) CredentialProviderPromoContext promoContext;

// The PromosManager is used to register the promo.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation CredentialProviderPromoMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          prefService:(PrefService*)prefService
                           localState:(PrefService*)localState {
  if (self = [super init]) {
    _prefService = prefService;
    _promosManager = promosManager;
    _localState = localState;
  }
  return self;
}

- (BOOL)canShowCredentialProviderPromoWithTrigger:
            (CredentialProviderPromoTrigger)trigger
                                        promoSeen:
                                            (BOOL)promoSeenInCurrentSession {
  BOOL impressionLimitMet =
      self.localState->GetBoolean(
          prefs::kIosCredentialProviderPromoStopPromo) ||
      (promoSeenInCurrentSession &&
       trigger != CredentialProviderPromoTrigger::RemindMeLater);
  return !impressionLimitMet && IsCredentialProviderExtensionPromoEnabled() &&
         !password_manager_util::IsCredentialProviderEnabledOnStartup(
             self.prefService);
}

- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger
                             context:(CredentialProviderPromoContext)context {
  CredentialProviderPromoSource source;
  self.promoContext = context;
  switch (trigger) {
    case CredentialProviderPromoTrigger::PasswordCopied:
      source = CredentialProviderPromoSource::kPasswordCopied;
      [self setAnimation];
      break;
    case CredentialProviderPromoTrigger::PasswordSaved:
      source = CredentialProviderPromoSource::kPasswordSaved;
      if (self.promoContext == CredentialProviderPromoContext::kLearnMore) {
        [self setAnimation];
      }
      break;
    case CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword:
      source = CredentialProviderPromoSource::kAutofillUsed;
      if (self.promoContext == CredentialProviderPromoContext::kLearnMore) {
        [self setAnimation];
      }
      break;
    case CredentialProviderPromoTrigger::RemindMeLater:
      // TODO(crbug.com/1392116): show the same promo that was shown when
      // 'remind me later' was selected
      source = CredentialProviderPromoSource::kRemindLaterSelected;
      [self setAnimation];
      break;
  }

  [self setTextAndImageWithSource:source];
}

- (void)registerPromoWithPromosManager {
  if (!self.promosManager || !IsFullscreenPromosManagerEnabled()) {
    return;
  }
  // TODO(crbug.com/1392116): register the promo with a 24 hour delay.
  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension);
}

#pragma mark - Private

// Sets the animation to the consumer that corresponds to the value of
// promoContext. Depending on the value of promoContext, either the 'first step'
// or 'learn more' animation path is set.
- (void)setAnimation {
  DCHECK(self.consumer);
  NSString* animationName;
  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    animationName = kFirstStepAnimation;
  } else {
    animationName = kLearnMoreAnimation;
  }
  NSString* path = [[NSBundle mainBundle] pathForResource:animationName
                                                   ofType:@"json"];
  [self.consumer setAnimation:path];
}

// Sets the text and image to the consumer. The text set depends on the value of
// promoContext. When `source` is kPasswordCopied, no image is set.
- (void)setTextAndImageWithSource:(CredentialProviderPromoSource)source {
  DCHECK(self.consumer);
  NSString* titleString;
  NSString* subtitleString;
  NSString* primaryActionString;
  UIImage* image;
  NSString* secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS);
  NSString* tertiaryActionString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_REMIND_ME_LATER);

  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    titleString =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_TITLE);
    subtitleString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_SUBTITLE);
    primaryActionString =
        l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_HOW);
    image = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kPasswordSuggestionKey);
    if (source == CredentialProviderPromoSource::kPasswordCopied ||
        source == CredentialProviderPromoSource::kRemindLaterSelected) {
      image = nil;
    }
  } else {
    titleString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_TITLE);
    subtitleString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_SUBTITLE);
    primaryActionString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_GO_TO_SETTINGS);
  }

  [self.consumer setTitleString:titleString
                 subtitleString:subtitleString
            primaryActionString:primaryActionString
          secondaryActionString:secondaryActionString
           tertiaryActionString:tertiaryActionString
                          image:image];
}

@end
