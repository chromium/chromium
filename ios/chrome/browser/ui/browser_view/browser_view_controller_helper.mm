// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/web/public/web_state.h"

namespace {
bookmarks::BookmarkModel* GetBookmarkModelForWebState(
    web::WebState* web_state) {
  if (!web_state)
    return nullptr;
  web::BrowserState* browser_state = web_state->GetBrowserState();
  if (!browser_state)
    return nullptr;
  return ios::BookmarkModelFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
}  // namespace

@implementation BrowserViewControllerHelper

- (BOOL)isToolbarLoading:(web::WebState*)webState {
  // Please note, this notion of isLoading is slightly different from WebState's
  // IsLoading().
  return webState && webState->IsLoading() &&
         !webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme);
}

- (BOOL)isWebStateBookmarked:(web::WebState*)webState {
  bookmarks::BookmarkModel* bookmarkModel =
      GetBookmarkModelForWebState(webState);
  return bookmarkModel &&
         bookmarkModel->IsBookmarked(webState->GetLastCommittedURL());
}

- (BOOL)isWebStateBookmarkedByUser:(web::WebState*)webState {
  bookmarks::BookmarkModel* bookmarkModel =
      GetBookmarkModelForWebState(webState);
  return bookmarkModel && bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
                              webState->GetLastCommittedURL());
}

@end
