// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

#include <utility>

#include "base/containers/contains.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace {

// Removes all subnodes of `node`, including `node`, that are in `bookmarks`.
void RemoveBookmarksRecursive(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* model,
    bookmarks::metrics::BookmarkEditSource source,
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  // Remove children in reverse order, so that the index remains valid.
  for (size_t i = node->children().size(); i > 0; --i) {
    RemoveBookmarksRecursive(bookmarks, model, source,
                             node->children()[i - 1].get(), location);
  }

  if (base::Contains(bookmarks, node)) {
    model->Remove(node, source, location);
  }
}

}  // namespace

LegacyBookmarkModel::LegacyBookmarkModel() = default;

LegacyBookmarkModel::~LegacyBookmarkModel() = default;

const bookmarks::BookmarkNode*
LegacyBookmarkModel::subtle_root_node_with_unspecified_children() const {
  return underlying_model()->root_node();
}

void LegacyBookmarkModel::Shutdown() {
  underlying_model()->Shutdown();
}

bool LegacyBookmarkModel::loaded() const {
  return underlying_model()->loaded();
}

void LegacyBookmarkModel::Remove(const bookmarks::BookmarkNode* node,
                                 bookmarks::metrics::BookmarkEditSource source,
                                 const base::Location& location) {
  underlying_model()->Remove(node, source, location);
}

void LegacyBookmarkModel::Move(const bookmarks::BookmarkNode* node,
                               const bookmarks::BookmarkNode* new_parent,
                               size_t index) {
  underlying_model()->Move(node, new_parent, index);
}

void LegacyBookmarkModel::Copy(const bookmarks::BookmarkNode* node,
                               const bookmarks::BookmarkNode* new_parent,
                               size_t index) {
  underlying_model()->Copy(node, new_parent, index);
}

void LegacyBookmarkModel::SetTitle(
    const bookmarks::BookmarkNode* node,
    const std::u16string& title,
    bookmarks::metrics::BookmarkEditSource source) {
  underlying_model()->SetTitle(node, title, source);
}

void LegacyBookmarkModel::SetURL(
    const bookmarks::BookmarkNode* node,
    const GURL& url,
    bookmarks::metrics::BookmarkEditSource source) {
  underlying_model()->SetURL(node, url, source);
}

void LegacyBookmarkModel::SetDateAdded(const bookmarks::BookmarkNode* node,
                                       base::Time date_added) {
  underlying_model()->SetDateAdded(node, date_added);
}

bool LegacyBookmarkModel::HasNoUserCreatedBookmarksOrFolders() const {
  return (!bookmark_bar_node() || bookmark_bar_node()->children().empty()) &&
         (!other_node() || other_node()->children().empty()) &&
         (!mobile_node() || mobile_node()->children().empty());
}

const bookmarks::BookmarkNode* LegacyBookmarkModel::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title) {
  return underlying_model()->AddFolder(parent, index, title);
}

const bookmarks::BookmarkNode* LegacyBookmarkModel::AddNewURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  return underlying_model()->AddNewURL(parent, index, title, url);
}

const bookmarks::BookmarkNode* LegacyBookmarkModel::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  return underlying_model()->AddURL(parent, index, title, url);
}

void LegacyBookmarkModel::RemoveMany(
    const std::set<const bookmarks::BookmarkNode*>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  RemoveBookmarksRecursive(nodes, underlying_model(), source,
                           underlying_model()->root_node(), location);
}

void LegacyBookmarkModel::CommitPendingWriteForTest() {
  underlying_model()->CommitPendingWriteForTest();
}
