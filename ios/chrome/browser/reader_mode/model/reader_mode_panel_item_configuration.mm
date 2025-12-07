// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_panel_item_configuration.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Activates Reader mode in the `web_state` if possible.
void ActivateReaderModeInWebState(base::WeakPtr<web::WebState> web_state) {
  if (!web_state || web_state->IsBeingDestroyed()) {
    return;
  }
  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state.get());
  if (reader_mode_tab_helper) {
    [reader_mode_tab_helper->GetReaderModeHandler()
        showReaderModeFromAccessPoint:ReaderModeAccessPoint::kContextualChip];
  }
}

}  // namespace

ReaderModePanelItemConfiguration::ReaderModePanelItemConfiguration(
    web::WebState* web_state)
    : ContextualPanelItemConfiguration(
          ContextualPanelItemType::ReaderModeItem) {
  entrypoint_message = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  entrypoint_message_large_entrypoint_always_shown = true;
  accessibility_label = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  accessibility_hint = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_HINT);
  entrypoint_image_name = base::SysNSStringToUTF8(GetReaderModeSymbolName());
  image_type = ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  relevance = ContextualPanelItemConfiguration::low_relevance - 1;
  entrypoint_custom_action =
      base::BindRepeating(&ActivateReaderModeInWebState, web_state->GetWeakPtr());

  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  DCHECK(reader_mode_tab_helper);
  reader_mode_tab_helper_observation_.Observe(reader_mode_tab_helper);
  web_state_observation_.Observe(web_state);
}

ReaderModePanelItemConfiguration::~ReaderModePanelItemConfiguration() = default;

#pragma mark - ContextualPanelItemConfiguration

void ReaderModePanelItemConfiguration::DidTransitionToSmallEntrypoint() {
}

#pragma mark - ReaderModeTabHelper::Observer

void ReaderModePanelItemConfiguration::ReaderModeTabHelperDestroyed(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
  reader_mode_tab_helper_observation_.Reset();
}

void ReaderModePanelItemConfiguration::ReaderModeWebStateDidLoadContent(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
}

void ReaderModePanelItemConfiguration::ReaderModeWebStateWillBecomeUnavailable(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state,
    ReaderModeDeactivationReason reason) {}

void ReaderModePanelItemConfiguration::ReaderModeDistillationFailed(
    ReaderModeTabHelper* tab_helper) {
  Invalidate();
}

#pragma mark - web::WebStateObserver

void ReaderModePanelItemConfiguration::WebStateDestroyed(
    web::WebState* web_state) {
  web_state_observation_.Reset();
}

void ReaderModePanelItemConfiguration::WasHidden(web::WebState* web_state) {
}

#pragma mark - Private

void ReaderModePanelItemConfiguration::Invalidate() {
  web::WebState* web_state = web_state_observation_.GetSource();
  if (!web_state || web_state->IsBeingDestroyed()) {
    return;
  }
  ContextualPanelTabHelper* contextual_panel_tab_helper =
      ContextualPanelTabHelper::FromWebState(web_state);
  if (contextual_panel_tab_helper) {
    contextual_panel_tab_helper->InvalidateContextualPanelItemConfiguration(
        this);
  }
}

bool ReaderModePanelItemConfiguration::IsProfileEligibleForBwg() {
  web::WebState* web_state = web_state_observation_.GetSource();
  if (!web_state || web_state->IsBeingDestroyed()) {
    return false;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  BwgService* bwg_service = BwgServiceFactory::GetForProfile(profile);
  return bwg_service && bwg_service->IsProfileEligibleForBwg();
}
