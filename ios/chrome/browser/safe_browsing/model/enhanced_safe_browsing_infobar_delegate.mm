// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

EnhancedSafeBrowsingInfobarDelegate::EnhancedSafeBrowsingInfobarDelegate(
    web::WebState* web_state,
    id<SettingsCommands> settings_commands_handler)
    : web_state_(web_state),
      settings_commands_handler_(settings_commands_handler) {}

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

#pragma mark - ConfirmInfoBarDelegate

infobars::InfoBarDelegate::InfoBarIdentifier
EnhancedSafeBrowsingInfobarDelegate::GetIdentifier() const {
  return ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE;
}

// Returns an empty message to satisfy implementation requirement for
// ConfirmInfoBarDelegate.
std::u16string EnhancedSafeBrowsingInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

bool EnhancedSafeBrowsingInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}
