// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_

#include "base/optional.h"
#include "components/omnibox/browser/suggestion_answer.h"

struct AutocompleteMatch;
class AutocompleteResult;
class GURL;
enum class WindowOpenDisposition;

class OmniboxPopupViewSuggestionsDelegate {
 public:
  // Called whenever the topmost suggestion image has changed.
  // Current UI should only use |matchType|; new UI may use |answerType| and
  // |faviconURL| if available.
  virtual void OnTopmostSuggestionImageChanged(
      AutocompleteMatchType::Type match_type,
      base::Optional<SuggestionAnswer::AnswerType> answer_type,
      GURL favicon_url) = 0;

  // Called when results are updated.
  virtual void OnResultsChanged(const AutocompleteResult& result) = 0;
  // Called whenever the popup is scrolled.
  virtual void OnPopupDidScroll() = 0;
  // Called when the user chooses a suggestion from the popup via the "append"
  // button.
  virtual void OnSelectedMatchForAppending(const base::string16& str) = 0;
  // Called when a match was chosen for opening.
  virtual void OnSelectedMatchForOpening(AutocompleteMatch match,
                                         WindowOpenDisposition disposition,
                                         const GURL& alternate_nav_url,
                                         const base::string16& pasted_text,
                                         size_t index) = 0;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
