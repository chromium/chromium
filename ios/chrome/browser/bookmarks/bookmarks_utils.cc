// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state) {
  BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(browser_state);

  if (!bookmark_model->loaded())
    return false;

  bookmark_model->RemoveAllUserBookmarks();

  for (const auto& child : bookmark_model->root_node()->children()) {
    if (!bookmark_model->client()->CanBeEditedByUser(child.get()))
      continue;
    if (!child->children().empty())
      return false;
  }

  // The default save folder is reset to the generic one.
  browser_state->GetPrefs()->SetInt64(prefs::kIosBookmarkFolderDefault, -1);
  return true;
}

std::vector<const BookmarkNode*> PrimaryPermanentNodes(BookmarkModel* model) {
  DCHECK(model->loaded());
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(model->mobile_node());
  nodes.push_back(model->bookmark_bar_node());
  nodes.push_back(model->other_node());
  return nodes;
}

bool IsPrimaryPermanentNode(const BookmarkNode* node, BookmarkModel* model) {
  std::vector<const BookmarkNode*> primary_nodes(PrimaryPermanentNodes(model));
  return base::Contains(primary_nodes, node);
}
