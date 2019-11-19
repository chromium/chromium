// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
#define IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_

#include <string>

#include "base/macros.h"

@class NSString;

namespace web {

class WebState;

class FindInPageManagerDelegate {
 public:
  FindInPageManagerDelegate() = default;

  // Called when a search for |query| finished with |match_count| found and all
  // matches were highlighted after calling FindInPageManager::Find() with
  // FindInPageSearch. Even if no matches are found, call will be made once a
  // find has completed, assuming it has not been interrupted by another find.
  // Will also be called if the total match count in the current page changes or
  // if FindInPageManager::StopFinding() is called.
  // Client should check |query| to ensure that it is processing |match_count|
  // for the correct find. |query| will be nil if responding to
  // FindInPageManager::StopFinding().
  virtual void DidHighlightMatches(WebState* web_state,
                                   int match_count,
                                   NSString* query) = 0;

  // Called when a match number |index| is selected with |context_string|
  // representing the text context of the match phrase. |context_string| can be
  // used in VoiceOver to notify the user of the context of the match in the
  // sentence. A selected match refers to a match that is
  // highlighted in a unique manner different from the other matches. This is
  // triggered by calling FindInPageManager::Find() with any FindInPageOptions
  // to indicate the new match number that was selected. This method is not
  // called if |FindInPageManager::Find| did not find any matches.
  virtual void DidSelectMatch(WebState* web_state,
                              int index,
                              NSString* context_string) = 0;

 protected:
  virtual ~FindInPageManagerDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindInPageManagerDelegate);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_FIND_IN_PAGE_FIND_IN_PAGE_MANAGER_DELEGATE_H_
