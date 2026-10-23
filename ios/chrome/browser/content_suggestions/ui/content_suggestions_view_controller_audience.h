// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_container_delegate.h"

enum class ContentSuggestionsModuleType;
enum class SafetyCheckItemType;
@class SetUpListItemView;
namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Audience for the ContentSuggestions, getting information from it.
@protocol
    ContentSuggestionsViewControllerAudience <MagicStackModuleContainerDelegate>

// Notifies the audience of the UIKit viewWillDisappear: callback.
- (void)viewWillDisappear;

// Called when a Safety Check item is selected by the user.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type;

// Indicates that the user has tapped the given `view`.
- (void)didTapSetUpListItemView:(SetUpListItemView*)view;

// Indicates that the user has tapped the given `tip`.
- (void)didSelectTip:(segmentation_platform::TipIdentifier)tip;

// Indicates that the user has tapped the App Bundle promo.
- (void)didSelectAppBundlePromo;

// Indicates that the user has tapped the Default Browser promo.
- (void)didTapDefaultBrowserPromo;

// Indicates that the user has tapped the AIM module.
- (void)didTapAIMModule;

// Notifies the audience that the content suggestions size has been updated.
- (void)contentSuggestionsWasUpdated;

// Indicates that the user has tapped the photo picker button in the actionable
// NTP Theme tip.
- (void)didSelectNTPThemePhotoPicker;

// Indicates that the user has tapped a theme option in the actionable NTP Theme
// tip.
- (void)didSelectNTPThemeOption;

// Notifies the audience that a user-uploaded background photo was saved from
// the actionable NTP Theme tip.
- (void)didSaveNTPThemeBackgroundPhoto;

// Indicates that the user has toggled the switch in the actionable Tips
// Notifications tip.
- (void)didToggleTipsNotificationsSwitch:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_AUDIENCE_H_
