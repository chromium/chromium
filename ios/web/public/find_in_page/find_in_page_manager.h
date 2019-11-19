// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_

#include "base/macros.h"
#import "ios/web/public/web_state_user_data.h"

@class NSString;

namespace web {

class FindInPageManagerDelegate;

// Indicates what action the FindinPageManager should take.
enum class FindInPageOptions {
  // Searches for a string. Highlights all matches. Selects and scrolls to the
  // first result if string is found. Selecting refers to highlighting in a
  // unique manner different from the other matches.
  FindInPageSearch = 1,
  // Selects and scrolls to the next result if there is one. Otherwise, nothing
  // will change. Loop back to the first result if currently on last result. If
  // passed before a Find() with FindInPageSearch call, nothing will change.
  FindInPageNext,
  // Selects and scrolls to the previous result if there is one. Otherwise,
  // nothing will change. Loop to last result if currently on first result. If
  // passed before a Find() with FindInPageSearch call, nothing will change.
  FindInPagePrevious,
};

// Manager for searching text on a page. Supports searching within all iframes.
class FindInPageManager : public web::WebStateUserData<FindInPageManager> {
 public:
  // Searches for string |query|. Executes new search or traverses results based
  // on |options|. |query| must not be null if |options| is |FindInPageSearch|.
  // |query| is ignored if |options| is not |FindInPageSearch|. If new search is
  // started before previous search finishes, old request will be discarded.
  // Check CanSearchContent() before calling Find().
  //
  // FindInPageManagerDelegate::DidHighlightMatches() will be called to return
  // the total matches found if FindInPageSearch is passed, assuming it hasn't
  // been discarded. FindInPageManagerDelegate::DidSelectMatch() will also be
  // called if matches were found to inform client of the new match that was
  // highlighted for all FindInPageOptions.
  virtual void Find(NSString* query, FindInPageOptions options) = 0;

  // Removes any highlighting. Does nothing if Find() with
  // FindInPageOptions::FindInPageSearch is never called.
  virtual void StopFinding() = 0;

  // Returns false if page content can not be searched (for example: an image)
  // or if search is not supported (for example: PDF files).
  virtual bool CanSearchContent() = 0;

  virtual FindInPageManagerDelegate* GetDelegate() = 0;
  virtual void SetDelegate(FindInPageManagerDelegate* delegate) = 0;

  ~FindInPageManager() override {}

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  FindInPageManager() {}

  DISALLOW_COPY_AND_ASSIGN(FindInPageManager);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_H_
