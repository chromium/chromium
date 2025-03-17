// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#import <memory>
#import <string>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/url_request/url_request_context_getter.h"

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxController* controller,
    OmniboxAutocompleteController* omnibox_autocomplete_controller)
    : OmniboxPopupView(controller),
      omnibox_autocomplete_controller_(omnibox_autocomplete_controller) {
  DCHECK(controller);
  model()->set_popup_view(this);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  model()->set_popup_view(nullptr);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  [omnibox_autocomplete_controller_ updatePopupSuggestions];
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return omnibox_autocomplete_controller_.omniboxPopupController.hasSuggestions;
}

#pragma mark - OmniboxPopupProvider

bool OmniboxPopupViewIOS::IsPopupOpen() {
  return omnibox_autocomplete_controller_.omniboxPopupController.hasSuggestions;
}

void OmniboxPopupViewIOS::SetTextAlignment(NSTextAlignment alignment) {
  [omnibox_autocomplete_controller_.omniboxPopupController
      setTextAlignment:alignment];
}

void OmniboxPopupViewIOS::SetSemanticContentAttribute(
    UISemanticContentAttribute semanticContentAttribute) {
  [omnibox_autocomplete_controller_.omniboxPopupController
      setSemanticContentAttribute:semanticContentAttribute];
}

void OmniboxPopupViewIOS::SetHasThumbnail(bool has_thumbnail) {
  [omnibox_autocomplete_controller_.omniboxPopupController
      setHasThumbnail:has_thumbnail];
}
