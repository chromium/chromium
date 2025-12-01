// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

EnhancedSafeBrowsingInfobarDelegate::EnhancedSafeBrowsingInfobarDelegate(
    web::WebState* web_state,
    id<SettingsCommands> settings_commands_handler,
    EnhancedSafeBrowsingInfobarScenario scenario,
    const std::string& email)
    : web_state_(web_state),
      settings_commands_handler_(settings_commands_handler),
      scenario_(scenario),
      email_(email) {}

EnhancedSafeBrowsingInfobarDelegate::~EnhancedSafeBrowsingInfobarDelegate() =
    default;

void EnhancedSafeBrowsingInfobarDelegate::ShowSafeBrowsingSettings() {
  RecordInteraction(EnhancedSafeBrowsingInfobarInteraction::kTapped);
  [settings_commands_handler_ showSafeBrowsingSettings];
}

void EnhancedSafeBrowsingInfobarDelegate::RecordInteraction(
    EnhancedSafeBrowsingInfobarInteraction interaction) {
  base::UmaHistogramEnumeration("IOS.SafeBrowsing.Enhanced.Infobar.Interaction",
                                interaction);
}

EnhancedSafeBrowsingIconType EnhancedSafeBrowsingInfobarDelegate::GetIconType()
    const {
  switch (scenario_) {
    case EnhancedSafeBrowsingInfobarScenario::kAccountSync:
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled:
      return EnhancedSafeBrowsingIconType::kShield;
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton:
      return EnhancedSafeBrowsingIconType::kInfo;
  }
}

int EnhancedSafeBrowsingInfobarDelegate::GetButtons() const {
  // Always show a button, even if it's a no-op dismissal button, because the UI
  // always reserves space for a button.
  return BUTTON_OK;
}

std::u16string EnhancedSafeBrowsingInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (button != BUTTON_OK) {
    return std::u16string();
  }

  switch (scenario_) {
    case EnhancedSafeBrowsingInfobarScenario::kAccountSync:
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton:
      // Button navigates to Settings.
      return l10n_util::GetStringUTF16(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_BUTTON_TEXT);
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled:
      // No-op "Okay" button to dismiss.
      return l10n_util::GetStringUTF16(IDS_OK);
  }
}

#pragma mark - ConfirmInfoBarDelegate

infobars::InfoBarDelegate::InfoBarIdentifier
EnhancedSafeBrowsingInfobarDelegate::GetIdentifier() const {
  return ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE;
}

std::u16string EnhancedSafeBrowsingInfobarDelegate::GetTitleText() const {
  switch (scenario_) {
    case EnhancedSafeBrowsingInfobarScenario::kAccountSync:
      return l10n_util::GetStringUTF16(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_TITLE);
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled:
      return l10n_util::GetStringUTF16(
          IDS_IOS_SAFE_BROWSING_ENHANCED_ON_INFOBAR_TITLE);
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton:
      return l10n_util::GetStringUTF16(
          IDS_IOS_SAFE_BROWSING_ENHANCED_OFF_INFOBAR_TITLE);
  }
}

std::u16string EnhancedSafeBrowsingInfobarDelegate::GetMessageText() const {
  switch (scenario_) {
    case EnhancedSafeBrowsingInfobarScenario::kAccountSync:
      return l10n_util::GetStringUTF16(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_INFOBAR_DESCRIPTION);
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled:
      return base::UTF8ToUTF16(email_);
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton:
      return std::u16string();
  }
}

bool EnhancedSafeBrowsingInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}

bool EnhancedSafeBrowsingInfobarDelegate::Accept() {
  switch (scenario_) {
    case EnhancedSafeBrowsingInfobarScenario::kAccountSync:
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton:
      // Action: Navigate to settings.
      ShowSafeBrowsingSettings();
      return true;
    case EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled:
      // Action: No-op, just allow the banner to be dismissed.
      return true;
  }
}
