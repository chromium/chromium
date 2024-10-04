// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <string>

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/omnibox_popup_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_provider.h"

@class OmniboxPopupMediator;
class OmniboxController;
class OmniboxPopupViewSuggestionsDelegate;
struct AutocompleteMatch;

// iOS implementation of OmniboxPopupView.
class OmniboxPopupViewIOS : public OmniboxPopupView,
                            public OmniboxPopupMediatorDelegate,
                            public OmniboxPopupProvider {
 public:
  OmniboxPopupViewIOS(OmniboxController* controller,
                      OmniboxPopupViewSuggestionsDelegate* delegate);
  ~OmniboxPopupViewIOS() override;

  // OmniboxPopupView implementation.
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override {}
  void UpdatePopupAppearance() override;
  void ProvideButtonFocusHint(size_t line) override {}
  void OnMatchIconUpdated(size_t match_index) override {}
  void OnDragCanceled() override {}
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) override {}
  void AddPopupAccessibleNodeData(ui::AXNodeData* node_data) override {}
  std::u16string GetAccessibleButtonTextForResult(size_t line) override;

  // OmniboxPopupProvider implemetation.
  void SetTextAlignment(NSTextAlignment alignment) override;
  void SetSemanticContentAttribute(
      UISemanticContentAttribute semanticContentAttribute) override;
  bool IsPopupOpen() override;
  void SetHasThumbnail(bool has_thumbnail) override;

  // OmniboxPopupViewControllerDelegate implementation.
  bool IsStarredMatch(const AutocompleteMatch& match) const override;
  // `disposition` should be CURRENT_TAB is the match should be loaded,
  // SWITCH_TO_TAB if it should switch to this tab.
  void OnMatchSelected(const AutocompleteMatch& match,
                       size_t row,
                       WindowOpenDisposition disposition) override;
  void OnMatchSelectedForAppending(const AutocompleteMatch& match) override;
  void OnMatchSelectedForDeletion(const AutocompleteMatch& match) override;
  void OnScroll() override;
  void OnCallActionTap() override;

  void SetMediator(OmniboxPopupMediator* mediator) { mediator_ = mediator; }

 private:
  raw_ptr<OmniboxPopupViewSuggestionsDelegate> delegate_;  // weak
  OmniboxPopupMediator* mediator_;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_IOS_H_
