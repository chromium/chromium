// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#import <stdint.h>

#import <algorithm>
#import <memory>
#import <vector>

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/hash/hash.h"
#import "base/i18n/string_compare.h"
#import "base/metrics/user_metrics_action.h"
#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/query_parser/query_parser.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ui/bookmarks/undo_manager_wrapper.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/models/tree_node_iterator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace bookmark_utils_ios {

NSString* const kBookmarksSnackbarCategory = @"BookmarksSnackbarCategory";

absl::optional<NodeSet> FindNodesByIds(bookmarks::BookmarkModel* model,
                                       const std::set<int64_t>& ids) {
  DCHECK(model);
  NodeSet nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (ids.find(node->id()) == ids.end()) {
      continue;
    }

    nodes.insert(node);
    if (ids.size() == nodes.size()) {
      break;
    }
  }

  if (ids.size() != nodes.size()) {
    return absl::nullopt;
  }

  return nodes;
}

const BookmarkNode* FindNodeById(bookmarks::BookmarkModel* model, int64_t id) {
  DCHECK(model);
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->id() == id) {
      return node;
    }
  }

  return nullptr;
}

const BookmarkNode* FindFolderById(bookmarks::BookmarkModel* model,
                                   int64_t id) {
  const BookmarkNode* node = FindNodeById(model, id);
  return node && node->is_folder() ? node : nullptr;
}

NSString* TitleForBookmarkNode(const BookmarkNode* node) {
  NSString* title;

  if (node->type() == BookmarkNode::BOOKMARK_BAR) {
    title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARKS_BAR_TITLE);
  } else if (node->type() == BookmarkNode::MOBILE) {
    title = l10n_util::GetNSString(IDS_BOOKMARK_BAR_MOBILE_FOLDER_NAME);
  } else if (node->type() == BookmarkNode::OTHER_NODE) {
    title = l10n_util::GetNSString(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME);
  } else {
    title = base::SysUTF16ToNSString(node->GetTitle());
  }

  // Assign a default bookmark name if it is at top level.
  if (node->is_root() && ![title length]) {
    title = l10n_util::GetNSString(IDS_SYNC_DATATYPE_BOOKMARKS);
  }

  return title;
}

#pragma mark - Updating Bookmarks

// Deletes all subnodes of `node`, including `node`, that are in `bookmarks`.
void DeleteBookmarks(const std::set<const BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* model,
                     const BookmarkNode* node) {
  // Delete children in reverse order, so that the index remains valid.
  for (size_t i = node->children().size(); i > 0; --i) {
    DeleteBookmarks(bookmarks, model, node->children()[i - 1].get());
  }

  if (bookmarks.find(node) != bookmarks.end()) {
    model->Remove(node);
  }
}

// Creates a toast which will undo the changes made to the bookmark model if
// the user presses the undo button, and the UndoManagerWrapper allows the undo
// to go through.
MDCSnackbarMessage* CreateUndoToastWithWrapper(UndoManagerWrapper* wrapper,
                                               NSString* text) {
  // Create the block that will be executed if the user taps the undo button.
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = ^{
    if (![wrapper hasUndoManagerChanged]) {
      [wrapper undo];
    }
  };

  action.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_UNDO_BUTTON_TITLE);
  action.accessibilityIdentifier = @"Undo";
  action.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_UNDO_BUTTON_TITLE);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.category = kBookmarksSnackbarCategory;
  return message;
}

MDCSnackbarMessage* CreateOrUpdateBookmarkWithUndoToast(
    const BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const BookmarkNode* folder,
    bookmarks::BookmarkModel* bookmark_model,
    ChromeBrowserState* browser_state) {
  DCHECK(!node || node->is_url());
  std::u16string titleString = base::SysNSStringToUTF16(title);

  // If the bookmark has no changes supporting Undo, just bail out.
  if (node && node->GetTitle() == titleString && node->url() == url &&
      node->parent() == folder) {
    return nil;
  }

  // Secondly, create an Undo group for all undoable actions.
  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:browser_state];

  // Create or update the bookmark.
  [wrapper startGroupingActions];

  // Save the bookmark information.
  if (!node) {  // Create a new bookmark.
    bookmark_model->client()->RecordAction(
        base::UserMetricsAction("BookmarkAdded"));
    node = bookmark_model->AddNewURL(folder, folder->children().size(),
                                     titleString, url);
  } else {  // Update the information.
    bookmark_model->SetTitle(node, titleString,
                             bookmarks::metrics::BookmarkEditSource::kUser);
    bookmark_model->SetURL(node, url,
                           bookmarks::metrics::BookmarkEditSource::kUser);

    DCHECK(folder);
    DCHECK(!folder->HasAncestor(node));
    if (node->parent() != folder) {
      bookmark_model->Move(node, folder, folder->children().size());
    }
    DCHECK(node->parent() == folder);
  }

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text =
      l10n_util::GetNSString((node) ? IDS_IOS_BOOKMARK_NEW_BOOKMARK_UPDATED
                                    : IDS_IOS_BOOKMARK_NEW_BOOKMARK_CREATED);
  return CreateUndoToastWithWrapper(wrapper, text);
}

MDCSnackbarMessage* CreateBookmarkAtPositionWithUndoToast(
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    int position,
    bookmarks::BookmarkModel* bookmark_model,
    ChromeBrowserState* browser_state) {
  std::u16string titleString = base::SysNSStringToUTF16(title);

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:browser_state];
  [wrapper startGroupingActions];

  bookmark_model->client()->RecordAction(
      base::UserMetricsAction("BookmarkAdded"));
  const bookmarks::BookmarkNode* node = bookmark_model->AddNewURL(
      folder, folder->children().size(), titleString, url);
  bookmark_model->Move(node, folder, position);

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARK_CREATED);
  return CreateUndoToastWithWrapper(wrapper, text);
}

MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    size_t position,
    bookmarks::BookmarkModel* bookmark_model,
    ChromeBrowserState* browser_state) {
  DCHECK(node);
  DCHECK(folder);
  DCHECK(!folder->HasAncestor(node));
  // Early return if node is not valid.
  if (!node && !folder) {
    return nil;
  }

  size_t old_index = node->parent()->GetIndexOf(node).value();
  // Early return if no change in position.
  if (node->parent() == folder && old_index == position) {
    return nil;
  }

  // Secondly, create an Undo group for all undoable actions.
  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:browser_state];

  // Update the bookmark.
  [wrapper startGroupingActions];
  bookmark_model->Move(node, folder, position);

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARK_UPDATED);
  return CreateUndoToastWithWrapper(wrapper, text);
}

void DeleteBookmarks(const std::set<const BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* model) {
  DCHECK(model->loaded());
  DeleteBookmarks(bookmarks, model, model->root_node());
}

MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const BookmarkNode*>& nodes,
    bookmarks::BookmarkModel* model,
    ChromeBrowserState* browser_state) {
  size_t node_count = nodes.size();
  DCHECK_GT(node_count, 0u);

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:browser_state];

  // Delete the selected bookmarks.
  [wrapper startGroupingActions];
  bookmark_utils_ios::DeleteBookmarks(nodes, model);
  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text = nil;

  if (node_count == 1) {
    text = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_SINGLE_BOOKMARK_DELETE);
  } else {
    text =
        l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_NEW_MULTIPLE_BOOKMARK_DELETE,
                                base::NumberToString16(node_count));
  }

  return CreateUndoToastWithWrapper(wrapper, text);
}

bool MoveBookmarks(const std::set<const BookmarkNode*>& bookmarks,
                   bookmarks::BookmarkModel* model,
                   const BookmarkNode* folder) {
  bool did_perform_move = false;

  // Calling Move() on the model will triger observer methods to fire, one of
  // them may modify the passed in `bookmarks`. To protect against this scenario
  // a copy of the set is made first.
  const std::set<const BookmarkNode*> bookmarks_copy(bookmarks);
  for (const BookmarkNode* node : bookmarks_copy) {
    // The bookmarks model can change under us at any time, so we can't make
    // any assumptions.
    if (folder->HasAncestor(node)) {
      continue;
    }
    if (node->parent() != folder) {
      model->Move(node, folder, folder->children().size());
      did_perform_move = true;
    }
  }
  return did_perform_move;
}

MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    const std::set<const BookmarkNode*>& nodes,
    bookmarks::BookmarkModel* model,
    const BookmarkNode* folder,
    ChromeBrowserState* browser_state) {
  size_t node_count = nodes.size();
  DCHECK_GT(node_count, 0u);

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:browser_state];

  // Move the selected bookmarks.
  [wrapper startGroupingActions];
  bool did_perform_move =
      bookmark_utils_ios::MoveBookmarks(nodes, model, folder);
  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  if (!did_perform_move) {
    return nil;  // Don't return a snackbar when no real move as happened.
  }

  NSString* text = nil;
  if (node_count == 1) {
    text = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_SINGLE_BOOKMARK_MOVE);
  } else {
    text = l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_NEW_MULTIPLE_BOOKMARK_MOVE,
                                   base::NumberToString16(node_count));
  }

  return CreateUndoToastWithWrapper(wrapper, text);
}

#pragma mark - Useful bookmark manipulation.

namespace {

// Adds all children of `folder` that are not obstructed to `results`. They are
// placed immediately after `folder`, using a depth-first, then alphabetically
// ordering. `results` must contain `folder`.
void UpdateFoldersFromNode(const BookmarkNode* folder,
                           NodeVector* results,
                           const NodeSet& obstructions);

// Returns whether `folder` has an ancestor in any of the nodes in
// `bookmarkNodes`.
bool FolderHasAncestorInBookmarkNodes(const BookmarkNode* folder,
                                      const NodeSet& bookmarkNodes);

// Returns true if the node is not a folder, is not visible, or is an ancestor
// of any of the nodes in `obstructions`.
bool IsObstructed(const BookmarkNode* node, const NodeSet& obstructions);

// Comparator used to sort bookmarks. No folders are allowed.
class FolderNodeComparator {
 public:
  explicit FolderNodeComparator(icu::Collator* collator)
      : collator_(collator) {}

  // Returns true if `n1` precedes `n2`.
  bool operator()(const BookmarkNode* n1, const BookmarkNode* n2) {
    if (!collator_) {
      return n1->GetTitle() < n2->GetTitle();
    }
    return base::i18n::CompareString16WithCollator(*collator_, n1->GetTitle(),
                                                   n2->GetTitle()) == UCOL_LESS;
  }

 private:
  icu::Collator* collator_;
};

bool FolderHasAncestorInBookmarkNodes(const BookmarkNode* folder,
                                      const NodeSet& bookmarkNodes) {
  DCHECK(folder->is_folder());
  for (const BookmarkNode* node : bookmarkNodes) {
    if (folder->HasAncestor(node)) {
      return true;
    }
  }
  return false;
}

bool IsObstructed(const BookmarkNode* node, const NodeSet& obstructions) {
  if (!node->is_folder()) {
    return true;
  }
  if (!node->IsVisible()) {
    return true;
  }
  if (FolderHasAncestorInBookmarkNodes(node, obstructions)) {
    return true;
  }
  return false;
}

void UpdateFoldersFromNode(const BookmarkNode* folder,
                           NodeVector* results,
                           const NodeSet& obstructions) {
  std::vector<const BookmarkNode*> directDescendants;
  for (const auto& subfolder : folder->children()) {
    if (!IsObstructed(subfolder.get(), obstructions)) {
      directDescendants.push_back(subfolder.get());
    }
  }

  bookmark_utils_ios::SortFolders(&directDescendants);

  auto it = base::ranges::find(*results, folder);
  DCHECK(it != results->end());
  ++it;
  results->insert(it, directDescendants.begin(), directDescendants.end());

  // Recursively perform the operation on each direct descendant.
  for (auto* node : directDescendants) {
    UpdateFoldersFromNode(node, results, obstructions);
  }
}

}  // namespace

void SortFolders(NodeVector* vector) {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error)) {
    collator.reset(NULL);
  }
  std::sort(vector->begin(), vector->end(),
            FolderNodeComparator(collator.get()));
}

NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     bookmarks::BookmarkModel* model) {
  NodeVector primary_nodes = PrimaryPermanentNodes(model);
  NodeVector filtered_primary_nodes;
  for (auto* node : primary_nodes) {
    if (IsObstructed(node, obstructions)) {
      continue;
    }

    filtered_primary_nodes.push_back(node);
  }

  // Copy the results over.
  NodeVector results = filtered_primary_nodes;

  // Iterate over a static copy of the filtered, root folders.
  for (auto* node : filtered_primary_nodes) {
    UpdateFoldersFromNode(node, &results, obstructions);
  }

  return results;
}

// Whether `vector1` contains only elements of `vector2` in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2) {
  NodeVector::const_iterator it = vector2.begin();
  // Scan the first vector.
  for (const auto* node : vector1) {
    // Look for a match in the rest of the second vector. When found, advance
    // the iterator on vector2 to only focus on the remaining part of vector2,
    // so that ordering is verified.
    it = std::find(it, vector2.end(), node);
    if (it == vector2.end()) {
      return NO;
    }
    // If found in vector2, advance the iterator so that the match is only
    // matched once.
    it++;
  }
  return YES;
}

// Returns the indices in `vector2` of the items in `vector2` that are not
// present in `vector1`.
// `vector1` MUST be a subvector of `vector2` in the sense of `IsSubvector`.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2) {
  DCHECK(IsSubvectorOfNodes(vector1, vector2))
      << "Can't compute missing nodes between nodes among which the first is "
         "not a subvector of the second.";

  std::vector<NodeVector::size_type> missing_nodes_indices;
  // Keep an iterator on vector1.
  NodeVector::const_iterator it1 = vector1.begin();
  // Scan vector2, looking for vector1 elements.
  for (NodeVector::size_type i2 = 0; i2 != vector2.size(); i2++) {
    // When vector1 has been fully traversed, all remaining elements of vector2
    // are to be added to the missing nodes.
    // Otherwise, while the element of vector2 is not equal to the element the
    // iterator on vector1 is pointing to, add vector2 elements to the missing
    // nodes.
    if (it1 == vector1.end() || vector2[i2] != *it1) {
      missing_nodes_indices.push_back(i2);
    } else {
      // When there is a match between vector2 and vector1, advance the iterator
      // of vector1.
      it1++;
    }
  }
  return missing_nodes_indices;
}

#pragma mark - Cache position in table view.

NSArray* CreateBookmarkPath(bookmarks::BookmarkModel* model,
                            int64_t folder_id) {
  // Create an array with root node id, if folder_id == root node.
  if (model->root_node()->id() == folder_id) {
    return @[ [NSNumber numberWithLongLong:model->root_node()->id()] ];
  }

  const BookmarkNode* bookmark = FindFolderById(model, folder_id);
  if (!bookmark) {
    return nil;
  }

  NSMutableArray* bookmarkPath = [NSMutableArray array];
  [bookmarkPath addObject:[NSNumber numberWithLongLong:folder_id]];
  while (model->root_node()->id() != bookmark->id()) {
    bookmark = bookmark->parent();
    DCHECK(bookmark);
    [bookmarkPath addObject:[NSNumber numberWithLongLong:bookmark->id()]];
  }
  return [[bookmarkPath reverseObjectEnumerator] allObjects];
}

}  // namespace bookmark_utils_ios
