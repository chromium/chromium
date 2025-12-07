// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE_H_

#include <string>

#import "components/infobars/core/confirm_infobar_delegate.h"

@protocol SettingsCommands;
namespace web {
class WebState;
}

// Scenarios for when to show the Enhanced Safe Browsing infobar.
enum class EnhancedSafeBrowsingInfobarScenario {
  // Shown when account sync occurs.
  kAccountSync,
  // Shown when ESB is enabled by client-sync.
  kClientSyncEnabled,
  // Shown when ESB is disabled by client-sync, with a "Settings" button.
  kClientSyncDisabledWithButton,
};

// Used as enum for the IOS.SafeBrowsing.Enhanced.Infobar.Interaction histogram.
// Keep in sync with "IOSEnhancedSafeBrowseringInfobarInteraction"
// in tools/metrics/histograms/metadata/ios/enums.xml.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange
enum class EnhancedSafeBrowsingInfobarInteraction {
  kViewed = 0,
  kTapped = 1,
  kMaxValue = kTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

// Icon types for the Enhanced Safe Browsing infobar.
enum class EnhancedSafeBrowsingIconType {
  kShield,
  kInfo,
};

// Delegate for infobar that prompts users to learn more about Enhanced Safe
// Browsing and navgiates them to the Enhanced Safe Browsing settings page when
// the package(s) are tracked or untracked.
class EnhancedSafeBrowsingInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  EnhancedSafeBrowsingInfobarDelegate(
      web::WebState* web_state,
      id<SettingsCommands> settings_commands_handler,
      EnhancedSafeBrowsingInfobarScenario scenario,
      const std::string& email);

  ~EnhancedSafeBrowsingInfobarDelegate() override;

  // Records interactions with the infobar to an UMA histogram.
  void RecordInteraction(EnhancedSafeBrowsingInfobarInteraction interaction);

  // Returns the icon type that should be displayed for the current scenario.
  EnhancedSafeBrowsingIconType GetIconType() const;

  // ConfirmInfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;
  bool Accept() override;

 private:
  // Navigates the user to the Safe Browsing settings menu page.
  void ShowSafeBrowsingSettings();

  raw_ptr<web::WebState> web_state_ = nullptr;
  id<SettingsCommands> settings_commands_handler_;
  EnhancedSafeBrowsingInfobarScenario scenario_;
  std::string email_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_ENHANCED_SAFE_BROWSING_INFOBAR_DELEGATE_H_
