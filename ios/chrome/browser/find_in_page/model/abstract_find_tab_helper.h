// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_ABSTRACT_FIND_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_ABSTRACT_FIND_TAB_HELPER_H_

@class FindInPageModel;
@protocol FindInPageResponseDelegate;
@class NSString;

namespace web {
class WebState;
}

// Interface shared by JavaScriptFindTabHelper and FindTabHelper.
class AbstractFindTabHelper {
 public:
  enum FindDirection {
    FORWARD,
    REVERSE,
  };

  AbstractFindTabHelper() = default;

  AbstractFindTabHelper(const AbstractFindTabHelper&) = delete;
  AbstractFindTabHelper& operator=(const AbstractFindTabHelper&) = delete;

  virtual ~AbstractFindTabHelper() = default;

  // Sets the FindInPageResponseDelegate delegate to send responses to
  // StartFinding(), ContinueFinding(), and StopFinding().
  virtual void SetResponseDelegate(
      id<FindInPageResponseDelegate> response_delegate) = 0;

  // Starts an asynchronous Find operation. Highlights matches on the current
  // page.  Always searches in the FORWARD direction.
  virtual void StartFinding(NSString* search_string) = 0;

  // Runs an asynchronous Find operation. Highlights matches on the current
  // page. Uses the previously remembered search string and searches in the
  // given `direction`.
  virtual void ContinueFinding(FindDirection direction) = 0;

  // Stops any running find operations. Removes any highlighting from the
  // current page.
  virtual void StopFinding() = 0;

  // Returns the FindInPageModel that contains the latest find results.
  virtual FindInPageModel* GetFindResult() const = 0;

  // Returns true if the currently loaded page supports Find in Page.
  virtual bool CurrentPageSupportsFindInPage() const = 0;

  // Returns true if the Find in Page UI is currently visible.
  virtual bool IsFindUIActive() const = 0;

  // Marks the Find in Page UI as visible or not.  This method does not directly
  // show or hide the UI.  It simply acts as a marker for whether or not the UI
  // is visible.
  virtual void SetFindUIActive(bool active) = 0;

  // Saves the current find text to persistent storage.
  virtual void PersistSearchTerm() = 0;

  // Restores the current find text from persistent storage.
  virtual void RestoreSearchTerm() = 0;
};

// Get the concrete tab helper currently attached to the given `web_state`.
AbstractFindTabHelper* GetConcreteFindTabHelperFromWebState(
    web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_ABSTRACT_FIND_TAB_HELPER_H_
