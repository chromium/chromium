// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

void RecordBookmarkLaunch(BookmarkLaunchLocation launch_location) {
  DCHECK(launch_location < BOOKMARK_LAUNCH_LOCATION_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Stars.LaunchLocation", launch_location,
                            BOOKMARK_LAUNCH_LOCATION_COUNT);
}

bool RemoveAllUserBookmarksIOS(ios::ChromeBrowserState* browser_state) {
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

std::vector<const BookmarkNode*> RootLevelFolders(BookmarkModel* model) {
  std::vector<const BookmarkNode*> root_level_folders;

  // Find the direct folder children of the primary permanent nodes.
  std::vector<const BookmarkNode*> primary_permanent_nodes =
      PrimaryPermanentNodes(model);
  for (const BookmarkNode* parent : primary_permanent_nodes) {
    for (const auto& child : parent->children()) {
      if (child->is_folder() && child->IsVisible())
        root_level_folders.push_back(child.get());
    }
  }
  return root_level_folders;
}

bool IsPrimaryPermanentNode(const BookmarkNode* node, BookmarkModel* model) {
  std::vector<const BookmarkNode*> primary_nodes(PrimaryPermanentNodes(model));
  return base::Contains(primary_nodes, node);
}

const BookmarkNode* RootLevelFolderForNode(const BookmarkNode* node,
                                           BookmarkModel* model) {
  // This helper function doesn't work for managed bookmarks. This checks that
  // |node| is editable by the user, which currently covers all the other
  // bookmarks except the managed bookmarks.
  DCHECK(model->client()->CanBeEditedByUser(node));

  const std::vector<const BookmarkNode*> root_folders(RootLevelFolders(model));
  const BookmarkNode* top = node;
  while (top && !base::Contains(root_folders, top)) {
    top = top->parent();
  }
  return top;
}
