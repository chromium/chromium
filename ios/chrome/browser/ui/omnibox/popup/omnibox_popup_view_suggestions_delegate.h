// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_

#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct AutocompleteMatch;
class AutocompleteResult;
class GURL;
enum class WindowOpenDisposition;

class OmniboxPopupViewSuggestionsDelegate {
 public:
  // Called whenever the selected image has changed.
  // has_match indicates if there is a selected match. When there's no selected
  // match (for example, on NTP with no zero suggest, there's no default match),
  // values in match_type, answer_type, and favicon_url are invalid and a
  // default image should be used instead. Current UI should only use
  // |matchType|; new UI may use |answerType| and |faviconURL| if available.
  virtual void OnSelectedMatchImageChanged(
      bool has_match,
      AutocompleteMatchType::Type match_type,
      absl::optional<SuggestionAnswer::AnswerType> answer_type,
      GURL favicon_url) = 0;

  // Called when results are updated.
  virtual void OnResultsChanged(const AutocompleteResult& result) = 0;
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
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_VIEW_SUGGESTIONS_DELEGATE_H_
