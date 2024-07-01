// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/bookmarks/model/legacy_bookmark_model.h"

#include <utility>

#include "components/bookmarks/browser/bookmark_model.h"

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
  return (!subtle_bookmark_bar_node() ||
          subtle_bookmark_bar_node()->children().empty()) &&
         (!subtle_other_node() || subtle_other_node()->children().empty()) &&
         (!subtle_mobile_node() || subtle_mobile_node()->children().empty());
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

void LegacyBookmarkModel::CommitPendingWriteForTest() {
  underlying_model()->CommitPendingWriteForTest();
}
