// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/price_insights/model/price_insights_model.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of icons displayed in feature rows.
const CGFloat kFeatureRowIconSize = 20;

}  // namespace

@interface PageActionMenuMediator () <CRWWebStateObserver>
@end

@implementation PageActionMenuMediator {
  // The web state referenced by this mediator.
  raw_ptr<web::WebState> _webState;

  // Observer for the WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;

  // The `PrefService` used to store reminder data.
  raw_ptr<PrefService> _profilePrefs;

  // The service to retrieve default search engine URL.
  raw_ptr<TemplateURLService> _templateURLService;

  // The service for the Gemini floaty.
  raw_ptr<BwgService> _BWGService;

  // The tab helper for Reader mode.
  raw_ptr<ReaderModeTabHelper> _readerModeTabHelper;

  // The host content settings map for managing site permissions.
  raw_ptr<HostContentSettingsMap> _hostContentSettingsMap;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              profilePrefService:(PrefService*)profilePrefs
              templateURLService:(TemplateURLService*)templateURLService
                      BWGService:(BwgService*)BWGService
             readerModeTabHelper:(ReaderModeTabHelper*)readerModeTabHelper
          hostContentSettingsMap:
              (HostContentSettingsMap*)hostContentSettingsMap {
  self = [super init];
  if (self) {
    _webState = webState;
    _profilePrefs = profilePrefs;
    _templateURLService = templateURLService;
    _BWGService = BWGService;
    _readerModeTabHelper = readerModeTabHelper;
    _hostContentSettingsMap = hostContentSettingsMap;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)disconnect {
  _readerModeTabHelper = nullptr;
  [self detachFromWebState];
}

- (BOOL)isLensAvailableForProfile {
  return IsLensOverlayAvailable(_profilePrefs);
}

#pragma mark - PageActionMenuMutator

- (BOOL)isLensAvailableForTraitCollection:(UITraitCollection*)traitCollection {
  BOOL isLandscape = IsCompactHeight(traitCollection);
  return [self isLensAvailableForProfile] &&
         search::DefaultSearchProviderIsGoogle(_templateURLService) &&
         !isLandscape;
}

- (BOOL)isGeminiAvailable {
  return !_webState->IsLoading() &&
         _BWGService->IsBwgAvailableForWebState(_webState);
}

- (BOOL)isReaderModeAvailable {
  // TODO(crbug.com/447371545): Migrate Reader Mode to the feature type system.
  if (!_readerModeTabHelper) {
    return NO;
  }
  return base::FeatureList::IsEnabled(
             kEnableReaderModePageEligibilityForToolsMenu)
             ? _readerModeTabHelper->CurrentPageIsDistillable()
             : _readerModeTabHelper->CurrentPageIsEligibleForReaderMode();
}

- (BOOL)isReaderModeActive {
  if (!_readerModeTabHelper) {
    return NO;
  }
  return _readerModeTabHelper->IsActive();
}

- (BOOL)isFeatureAvailable:(PageActionMenuFeatureType)featureType {
  if (!_webState) {
    return NO;
  }

  switch (featureType) {
    case PageActionMenuTranslate: {
      ChromeIOSTranslateClient* translateClient =
          ChromeIOSTranslateClient::FromWebState(_webState);
      return IsTranslateActive(translateClient);
    }
    case PageActionMenuCameraPermission: {
      web::PermissionState state =
          _webState->GetStateForPermission(web::PermissionCamera);
      return state == web::PermissionStateAllowed;
    }
    case PageActionMenuMicrophonePermission: {
      web::PermissionState state =
          _webState->GetStateForPermission(web::PermissionMicrophone);
      return state == web::PermissionStateAllowed;
    }
    case PageActionMenuPopupBlocker: {
      if (!_hostContentSettingsMap) {
        return NO;
      }
      GURL url = _webState->GetLastCommittedURL();
      ContentSetting setting = _hostContentSettingsMap->GetContentSetting(
          url, url, ContentSettingsType::POPUPS);

      // Show row only if blocking is active AND there are blocked popups.
      BlockedPopupTabHelper* helper =
          BlockedPopupTabHelper::GetOrCreateForWebState(_webState);
      bool hasBlockedPopups = helper && helper->GetBlockedPopupCount() > 0;

      return setting == CONTENT_SETTING_BLOCK && hasBlockedPopups;
    }
    case PageActionMenuPriceTracking: {
      ContextualPanelTabHelper* tabHelper =
          ContextualPanelTabHelper::FromWebState(_webState);
      if (!tabHelper) {
        return NO;
      }

      std::vector<base::WeakPtr<ContextualPanelItemConfiguration>> configs =
          tabHelper->GetCurrentCachedConfigurations();

      for (const auto& config_weak : configs) {
        if (!config_weak) {
          continue;
        }

        ContextualPanelItemConfiguration* config = config_weak.get();
        if (config->item_type == ContextualPanelItemType::PriceInsightsItem) {
          return YES;
        }
      }

      return NO;
    }
  }
}

- (NSString*)translateLanguagePair {
  if (!_webState) {
    return nil;
  }

  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(_webState);

  if (!IsTranslateActive(translateClient)) {
    return nil;
  }

  std::string sourceCode = GetSourceLanguageCode(translateClient);
  std::string targetCode = GetTargetLanguageCode(translateClient);

  // TODO(crbug.com/447143165): Convert language codes to display names.
  return [NSString
      stringWithFormat:@"%s to %s", sourceCode.c_str(), targetCode.c_str()];
}

- (NSInteger)blockedPopupCount {
  if (!_webState) {
    return 0;
  }
  BlockedPopupTabHelper* helper =
      BlockedPopupTabHelper::GetOrCreateForWebState(_webState);
  return helper ? helper->GetBlockedPopupCount() : 0;
}

- (NSString*)currentSiteDomain {
  if (!_webState) {
    return nil;
  }
  GURL url = _webState->GetLastCommittedURL();
  return base::SysUTF8ToNSString(url.GetHost());
}

- (void)revokePermission:(PageActionMenuFeatureType)featureType {
  if (!_webState) {
    return;
  }

  web::Permission permission;
  switch (featureType) {
    case PageActionMenuCameraPermission:
      permission = web::PermissionCamera;
      break;
    case PageActionMenuMicrophonePermission:
      permission = web::PermissionMicrophone;
      break;
    case PageActionMenuTranslate:
    case PageActionMenuPopupBlocker:
    case PageActionMenuPriceTracking:
      CHECK(false)
          << "revokePermission called with non-permission feature type: "
          << featureType;
  }

  _webState->SetStateForPermission(web::PermissionStateBlocked, permission);

  // Post task to ensure WebState has processed the permission change.
  __weak PageActionMenuMediator* weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf.consumer updateFeatureRowsAvailability];
      }));
}

- (NSArray<PageActionMenuFeature*>*)activeFeatures {
  NSMutableArray<PageActionMenuFeature*>* features =
      [[NSMutableArray alloc] init];

  // Translate feature.
  if ([self isFeatureAvailable:PageActionMenuTranslate]) {
    PageActionMenuFeature* translateFeature = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuTranslate
                      title:l10n_util::GetNSString(
                                IDS_IOS_AI_HUB_TRANSLATE_LABEL)
                       icon:DefaultSymbolWithPointSize(kTranslateSymbol,
                                                       kFeatureRowIconSize)
                 actionType:PageActionMenuButtonAction];
    translateFeature.subtitle = [self translateLanguagePair];
    translateFeature.actionText = l10n_util::GetNSString(
        IDS_IOS_AI_HUB_TRANSLATE_SHOW_ORIGINAL_BUTTON_LABEL);
    [features addObject:translateFeature];
  }

  // Popup blocker feature.
  if ([self isFeatureAvailable:PageActionMenuPopupBlocker]) {
    PageActionMenuFeature* popupFeature = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuPopupBlocker
                      title:l10n_util::GetNSString(
                                IDS_IOS_AI_HUB_POPUP_BLOCKER_LABEL)
                       icon:CustomSymbolWithPointSize(kPopupBadgeMinusSymbol,
                                                      kFeatureRowIconSize)
                 actionType:PageActionMenuButtonAction];

    NSInteger blockedCount = [self blockedPopupCount];
    NSString* countString =
        [NSString stringWithFormat:@"%ld", (long)blockedCount];
    popupFeature.subtitle =
        l10n_util::GetNSStringF(IDS_IOS_AI_HUB_POPUP_BLOCKER_COUNT_SUBTITLE,
                                base::SysNSStringToUTF16(countString));
    popupFeature.actionText =
        l10n_util::GetNSString(IDS_IOS_AI_HUB_POPUP_ALWAYS_SHOW_BUTTON_LABEL);
    [features addObject:popupFeature];
  }

  // Camera permission feature.
  if ([self isFeatureAvailable:PageActionMenuCameraPermission]) {
    PageActionMenuFeature* cameraFeature = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuCameraPermission
                      title:l10n_util::GetNSString(
                                IDS_IOS_AI_HUB_CAMERA_PERMISSION_LABEL)
                       icon:CustomSymbolWithPointSize(kCameraFillSymbol,
                                                      kFeatureRowIconSize)
                 actionType:PageActionMenuToggleAction];
    cameraFeature.toggleState = YES;
    [features addObject:cameraFeature];
  }

  // Microphone permission feature.
  if ([self isFeatureAvailable:PageActionMenuMicrophonePermission]) {
    PageActionMenuFeature* micFeature = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuMicrophonePermission
                      title:l10n_util::GetNSString(
                                IDS_IOS_AI_HUB_MICROPHONE_PERMISSION_LABEL)
                       icon:DefaultSymbolWithPointSize(kMicrophoneFillSymbol,
                                                       kFeatureRowIconSize)
                 actionType:PageActionMenuToggleAction];
    micFeature.toggleState = YES;
    [features addObject:micFeature];
  }
  // Price tracking feature.
  if ([self isFeatureAvailable:PageActionMenuPriceTracking]) {
    PageActionMenuFeature* priceTrackingFeature = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuPriceTracking
                      title:l10n_util::GetNSString(
                                IDS_IOS_AI_HUB_PRICE_TRACKING_LABEL)
                       icon:CustomSymbolWithPointSize(kDownTrendSymbol,
                                                      kFeatureRowIconSize)
                 actionType:PageActionMenuButtonAction];
    priceTrackingFeature.actionText =
        l10n_util::GetNSString(IDS_IOS_AI_HUB_PRICE_TRACKING_BUTTON_LABEL);
    [features addObject:priceTrackingFeature];
  }

  return features;
}

- (void)allowBlockedPopups {
  if (!_webState || !_hostContentSettingsMap) {
    return;
  }

  BlockedPopupTabHelper* helper =
      BlockedPopupTabHelper::GetOrCreateForWebState(_webState);
  if (!helper) {
    return;
  }

  // Get the blocked popups.
  std::vector<BlockedPopupTabHelper::Popup> popups = helper->GetBlockedPopups();

  GURL currentUrl = _webState->GetLastCommittedURL();

  // Open each blocked popup and allow future popups from this site.
  for (const auto& popup : popups) {
    web::WebState::OpenURLParams params(popup.popup_url, popup.referrer,
                                        WindowOpenDisposition::NEW_POPUP,
                                        ui::PAGE_TRANSITION_LINK, true);
    _webState->OpenURL(params);

    // Set popup permission to ALLOW for the referrer site.
    _hostContentSettingsMap->SetContentSettingCustomScope(
        ContentSettingsPattern::FromURL(popup.referrer.url),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
        CONTENT_SETTING_ALLOW);
  }

  [self.consumer updateFeatureRowsAvailability];
}

- (void)revertTranslation {
  if (!_webState) {
    return;
  }

  ChromeIOSTranslateClient* translateClient =
      ChromeIOSTranslateClient::FromWebState(_webState);
  if (!translateClient || !translateClient->GetTranslateManager()) {
    return;
  }

  translateClient->GetTranslateManager()->RevertTranslation();

  [self.consumer updateFeatureRowsAvailability];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self.consumer pageLoadStatusChanged];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self.consumer pageLoadStatusChanged];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self disconnect];
  [self.pageActionMenuHandler dismissPageActionMenuWithCompletion:nil];
}

#pragma mark - Private

// Stops observing the associated WebState, resets it and resets the observation
// bridge.
- (void)detachFromWebState {
  if (_webState) {
    if (_webStateObserver) {
      _webState->RemoveObserver(_webStateObserver.get());
    }
    _webState = nullptr;
  }
  _webStateObserver.reset();
}

// Returns true if translation is currently active for the page.
bool IsTranslateActive(ChromeIOSTranslateClient* translate_client) {
  return translate_client && translate_client->GetTranslateManager() &&
         translate_client->GetTranslateManager()
             ->GetLanguageState()
             ->IsPageTranslated();
}

// Returns the source language code of the translated page.
std::string GetSourceLanguageCode(ChromeIOSTranslateClient* translate_client) {
  if (!translate_client || !translate_client->GetTranslateManager()) {
    return "";
  }
  return translate::TranslateDownloadManager::GetLanguageCode(
      translate_client->GetTranslateManager()
          ->GetLanguageState()
          ->source_language());
}

// Returns the target language code of the translated page.
std::string GetTargetLanguageCode(ChromeIOSTranslateClient* translate_client) {
  if (!translate_client || !translate_client->GetTranslateManager()) {
    return "";
  }
  return translate_client->GetTranslateManager()
      ->GetLanguageState()
      ->current_language();
}

- (void)openPriceInsightsPanel {
  if (!_contextualSheetHandler) {
    return;
  }

  [self.contextualSheetHandler openContextualSheet];
}

@end
