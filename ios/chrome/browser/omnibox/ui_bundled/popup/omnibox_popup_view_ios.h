// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/omnibox_popup_view.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_provider.h"

@class OmniboxAutocompleteController;
@class OmniboxPopupMediator;
class OmniboxController;
struct AutocompleteMatch;

// iOS implementation of OmniboxPopupView.
class OmniboxPopupViewIOS : public OmniboxPopupView,
                            public OmniboxPopupProvider {
 public:
  OmniboxPopupViewIOS(
      OmniboxController* controller,
      OmniboxAutocompleteController* omniboxAutocompleteController);
  ~OmniboxPopupViewIOS() override;

  // OmniboxPopupView implementation.
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override {}
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override {}
  void OnMatchIconUpdated(size_t match_index) override {}
  void OnDragCanceled() override {}
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override {}

  // OmniboxPopupProvider implemetation.
  void SetTextAlignment(NSTextAlignment alignment) override;
  void SetSemanticContentAttribute(
      UISemanticContentAttribute semanticContentAttribute) override;
  bool IsPopupOpen() override;
  void SetHasThumbnail(bool has_thumbnail) override;

 private:
  __weak OmniboxAutocompleteController* omnibox_autocomplete_controller_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_
