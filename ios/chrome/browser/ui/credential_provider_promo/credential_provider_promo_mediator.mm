// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"

#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/credential_provider_promo/features.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
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

// Indicates whether the 'first step' or 'learn more' version of the promo is
// being presented.
@property(nonatomic, assign) CredentialProviderPromoContext promoContext;

// The PromosManager is used to register the promo.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation CredentialProviderPromoMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          prefService:(PrefService*)prefService {
  if (self = [super init]) {
    _prefService = prefService;
    _promosManager = promosManager;
  }
  return self;
}

- (BOOL)canShowCredentialProviderPromoWithTrigger:
            (CredentialProviderPromoTrigger)trigger
                                        promoSeen:
                                            (BOOL)promoSeenInCurrentSession {
  if (trigger == CredentialProviderPromoTrigger::SetUpList) {
    // Always allow showing when triggered by user via the SetUpList.
    return YES;
  }
  BOOL impressionLimitMet =
      GetApplicationContext()->GetLocalState()->GetBoolean(
          prefs::kIosCredentialProviderPromoStopPromo) ||
      (promoSeenInCurrentSession &&
       trigger != CredentialProviderPromoTrigger::RemindMeLater);
  BOOL policyEnabled = GetApplicationContext()->GetLocalState()->GetBoolean(
      prefs::kIosCredentialProviderPromoPolicyEnabled);
  return !impressionLimitMet && IsCredentialProviderExtensionPromoEnabled() &&
         policyEnabled &&
         !password_manager_util::IsCredentialProviderEnabledOnStartup(
             self.prefService);
}

- (void)configureConsumerWithTrigger:(CredentialProviderPromoTrigger)trigger
                             context:(CredentialProviderPromoContext)context {
  IOSCredentialProviderPromoSource source;
  self.promoContext = context;
  switch (trigger) {
    case CredentialProviderPromoTrigger::PasswordCopied:
      source = IOSCredentialProviderPromoSource::kPasswordCopied;
      [self setAnimation];
      break;
    case CredentialProviderPromoTrigger::PasswordSaved:
      source = IOSCredentialProviderPromoSource::kPasswordSaved;
      if (self.promoContext == CredentialProviderPromoContext::kLearnMore) {
        [self setAnimation];
      }
      break;
    case CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword:
      source = IOSCredentialProviderPromoSource::kAutofillUsed;
      if (self.promoContext == CredentialProviderPromoContext::kLearnMore) {
        [self setAnimation];
      }
      break;
    case CredentialProviderPromoTrigger::RemindMeLater:
      source = [self promoOriginalSource];

      // Reset the state. This case `RemindMeLater` implicitly means it is
      // presented from and would be deregistered by the Promo Manager
      // internally.
      GetApplicationContext()->GetLocalState()->SetBoolean(
          prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager,
          false);

      [self setAnimation];
      break;
    case CredentialProviderPromoTrigger::SetUpList:
      source = IOSCredentialProviderPromoSource::kSetUpList;
      [self setAnimation];
      break;
  }

  [self setTextAndImageWithSource:source];

  // Set the promo source in the Prefs. Skip for 'RemindMeLater' triggers as
  // source is already present.
  if (trigger != CredentialProviderPromoTrigger::RemindMeLater) {
    GetApplicationContext()->GetLocalState()->SetInteger(
        prefs::kIosCredentialProviderPromoSource, static_cast<int>(source));
  }
}

- (void)registerPromoWithPromosManager {
  if (!self.promosManager) {
    return;
  }
  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::CredentialProviderExtension, base::Hours(24));

  if (self.tracker) {
    self.tracker->NotifyEvent(
        feature_engagement::events::kCredentialProviderExtensionPromoSnoozed);
  }

  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager, true);
}

- (IOSCredentialProviderPromoSource)promoOriginalSource {
  int sourceAsInteger = GetApplicationContext()->GetLocalState()->GetInteger(
      prefs::kIosCredentialProviderPromoSource);
  DCHECK(sourceAsInteger <=
         static_cast<int>(IOSCredentialProviderPromoSource::kMaxValue));
  return static_cast<IOSCredentialProviderPromoSource>(sourceAsInteger);
}

#pragma mark - Private

// Sets the animation to the consumer that corresponds to the value of
// promoContext. Depending on the value of promoContext, either the 'first step'
// or 'learn more' animation name is set.
- (void)setAnimation {
  DCHECK(self.consumer);
  NSString* animationName;
  if (self.promoContext == CredentialProviderPromoContext::kFirstStep) {
    animationName = kFirstStepAnimation;
  } else {
    animationName = kLearnMoreAnimation;
  }
  [self.consumer setAnimation:animationName];
}

// Sets the text and image to the consumer. The text set depends on the value of
// promoContext. When `source` is kPasswordCopied, no image is set.
- (void)setTextAndImageWithSource:(IOSCredentialProviderPromoSource)source {
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
    if (source == IOSCredentialProviderPromoSource::kPasswordCopied) {
      image = nil;
    }
  } else {
    titleString = l10n_util::GetNSString(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_TITLE);
    NSString* settingsMenuItemString = nil;
    if (@available(iOS 16, *)) {
      settingsMenuItemString = l10n_util::GetNSString(
          IDS_IOS_CREDENTIAL_PROVIDER_PROMO_OS_PASSWORDS_SETTINGS_TITLE_IOS16);
    } else {
      settingsMenuItemString = l10n_util::GetNSString(
          IDS_IOS_CREDENTIAL_PROVIDER_PROMO_OS_PASSWORDS_SETTINGS_TITLE_BELOW_IOS16);
    }
    DCHECK(settingsMenuItemString.length > 0);
    subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_SUBTITLE_WITH_PH,
        base::SysNSStringToUTF16(settingsMenuItemString));
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
