// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_

#include "components/omnibox/browser/suggestion_answer.h"

struct AutocompleteMatch;
class GURL;
enum class WindowOpenDisposition;

class OmniboxPopupViewSuggestionsDelegate {
 public:
  // Called whenever the popup is scrolled.
  virtual void OnPopupDidScroll() = 0;
  // Called when the user chooses a suggestion from the popup via the "append"
  // button.
  virtual void OnSelectedMatchForAppending(const std::u16string& str) = 0;
  // Called when a match was chosen for opening.
  virtual void OnSelectedMatchForOpening(AutocompleteMatch match,
                                         WindowOpenDisposition disposition,
                                         const GURL& alternate_nav_url,
                                         const std::u16string& pasted_text,
                                         size_t index) = 0;
  // Called when a call action was tapped.
  virtual void OnCallActionTap() = 0;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
