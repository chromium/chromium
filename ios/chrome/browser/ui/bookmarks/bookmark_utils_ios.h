// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/common/storage_type.h"

class ChromeBrowserState;
class GURL;
@class MDCSnackbarMessage;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}  // namespace syncer

namespace bookmark_utils_ios {

// This class holds a node id and its bookmark model.
struct BookmarkNodeReference {
  BookmarkNodeReference(const base::Uuid& uuid,
                        bookmarks::BookmarkModel* bookmark_model);
  BookmarkNodeReference(const BookmarkNodeReference&);
  ~BookmarkNodeReference();

  // This operator is needed to be used `BookmarkNodeReference` in a std::set.
  bool operator<(const BookmarkNodeReference reference) const;
  BookmarkNodeReference& operator=(const BookmarkNodeReference& other) = delete;

  // Node id for the BookmarkNode.
  const base::Uuid uuid;
  // Bookmark model from the BookmarkNode.
  bookmarks::BookmarkModel* bookmark_model;
};

typedef std::vector<const bookmarks::BookmarkNode*> NodeVector;
typedef std::set<const bookmarks::BookmarkNode*> NodeSet;
typedef std::set<BookmarkNodeReference> NodeReferenceSet;

// Converts a set of BookmarkNode into a set of BookmarkNodeReference.
NodeReferenceSet FindNodeReferenceByNodes(
    NodeSet nodes,
    bookmarks::BookmarkModel* profile_bookmark_model,
    bookmarks::BookmarkModel* account_bookmark_model);

// Converts a BookmarkNodeReference into a BookmarkNode. This function might
// returns `nullptr` if the bookmark node doesn't exist anymore.
const bookmarks::BookmarkNode* FindNodeByNodeReference(
    BookmarkNodeReference reference);

// Converts a set of BookmarkNodeReference into a set of BookmarkNode. This
// function might return fewer BookmarkNodeReference objects than BookmarkNode
// if the nodes don't exist anymore.
NodeSet FindNodesByNodeReferences(NodeReferenceSet references);

// Finds bookmark node passed in `id`, in the `model`.
const bookmarks::BookmarkNode* FindNodeById(bookmarks::BookmarkModel* model,
                                            int64_t id);

// Finds bookmark node passed in `id`, in the `model`. Returns null if the
// node is found but not a folder.
const bookmarks::BookmarkNode* FindFolderById(bookmarks::BookmarkModel* model,
                                              int64_t id);

// The iOS code is doing some munging of the bookmark folder names in order
// to display a slighly different wording for the default folders.
NSString* TitleForBookmarkNode(const bookmarks::BookmarkNode* node);

// Returns the model type for a node, based on profile model and account model,
// based on the root node.
// `bookmark_node` is the bookmark to query. It can not be null.
// `profile_model` is the profile mode. It can not be null.
// `account_model` is the account mode. It can be null.
// The node must belong to one of the two models.
// This function is linear in time in the depth of the bookmark_node.
// TODO(crbug.com/1417992): once the bookmark nodes has access to its model,
// rewrite the function to be constant time.
bookmarks::StorageType GetBookmarkModelType(
    const bookmarks::BookmarkNode* bookmark_node,
    bookmarks::BookmarkModel* profile_model,
    bookmarks::BookmarkModel* account_model);

// Returns the bookmark model for a node, based on profile model and account
// model.
// `bookmark_node` is the bookmark to query. It can not be null.
// `profile_model` is the profile mode. It can not be null.
// `account_model` is the account mode. It can be null.
// The node must belong to one of the two models.
// This function is linear in time in the depth of the bookmark_node because it
// uses `GetBookmarkModelType(...)`.
// TODO(crbug.com/1417992): once the bookmark nodes has access to its model,
// rewrite the function to be constant time.
bookmarks::BookmarkModel* GetBookmarkModelForNode(
    const bookmarks::BookmarkNode* bookmark_node,
    bookmarks::BookmarkModel* profile_model,
    bookmarks::BookmarkModel* account_model);

// Returns true if the user is signed in and they opted in for the account
// bookmark storage.
bool IsAccountBookmarkStorageOptedIn(syncer::SyncService* sync_service);

// Creates the bookmark if `node` is NULL. Otherwise updates `node`.
// `folder` is the intended parent of `node`.
// Returns a boolean signifying whether any change was performed.
// Note: This function might invalidate `node` if `folder` and `node` belong to
// different `BookmarkModel` instances.
bool CreateOrUpdateBookmark(const bookmarks::BookmarkNode* node,
                            NSString* title,
                            const GURL& url,
                            const bookmarks::BookmarkNode* folder,
                            bookmarks::BookmarkModel* local_or_syncable_model,
                            bookmarks::BookmarkModel* account_model);

// Similar to `CreateOrUpdateBookmark`, but returns a snackbar that allows to
// undo the performed action. Returns nil if there's nothing to undo.
// Note: This function might invalidate `node` if `folder` and `node` belong to
// different `BookmarkModel` instances.
// TODO(crbug.com/1099901): Refactor to include position and replace two
// functions below.
MDCSnackbarMessage* CreateOrUpdateBookmarkWithUndoToast(
    const bookmarks::BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    bookmarks::BookmarkModel* local_or_syncable_model,
    bookmarks::BookmarkModel* account_model,
    ChromeBrowserState* browser_state);

// Creates a new bookmark with `title`, `url`, at `position` under parent
// `folder`. Returns a snackbar with an undo action. Returns nil if operation
// failed or there's nothing to undo.
MDCSnackbarMessage* CreateBookmarkAtPositionWithUndoToast(
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    int position,
    bookmarks::BookmarkModel* local_or_syncable_model,
    bookmarks::BookmarkModel* account_model,
    ChromeBrowserState* browser_state);

// Updates a bookmark node position, and returns a snackbar with an undo action.
// `node` and `folder` must belong to the same model. Returns nil if the
// operation wasn't successful or there's nothing to undo.
MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    size_t position,
    bookmarks::BookmarkModel* local_or_syncable_model,
    bookmarks::BookmarkModel* account_model,
    ChromeBrowserState* browser_state);

// Deletes all nodes in `bookmarks` from models in `bookmark_models` that are
// in `bookmarks`, and returns a snackbar with an undo action. Returns nil if
// the operation wasn't successful or there's nothing to undo.
MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    const std::vector<bookmarks::BookmarkModel*>& bookmark_models,
    ChromeBrowserState* browser_state);

// Deletes all nodes in `bookmarks`.
void DeleteBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* model);

// Move all `bookmarks_to_move` to the given `folder`, and returns a snackbar
// with an undo action. Returns nil if the operation wasn't successful or
// there's nothing to undo.
// This method updates `bookmarks_to_move` with new pointers to moved nodes, see
// `MoveBookmarks` documentation for details.
MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* local_model,
    bookmarks::BookmarkModel* account_model,
    const bookmarks::BookmarkNode* destination_folder,
    ChromeBrowserState* browser_state);

// Move all `bookmarks` to the given `folder`.
// Returns whether this method actually moved bookmarks (for example, only
// moving a folder to its parent will return `false`).
// This method updates `bookmarks_to_move` with new pointers to moved nodes. In
// other words, when the node contained in `bookmarks_to_move` at index N is
// moved - the updated `BookmarkNode` pointer is saved in `bookmarks_to_move` at
// the same index N.
bool MoveBookmarks(
    std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* local_model,
    bookmarks::BookmarkModel* account_model,
    const bookmarks::BookmarkNode* destination_folder);

// Category name for all bookmarks related snackbars.
extern NSString* const kBookmarksSnackbarCategory;

// Sorts a vector full of folders by title.
void SortFolders(NodeVector* vector);

// Returns a vector of root level folders and all their folder descendants,
// sorted depth-first, then alphabetically. The returned nodes are visible, and
// are guaranteed to not be descendants of any nodes in `obstructions`.
NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     bookmarks::BookmarkModel* model);

// Whether `vector1` contains only elements of `vector2` in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2);

// Returns the indices in `vector2` of the items in `vector2` that are not
// present in `vector1`.
// `vector1` MUST be a subvector of `vector2` in the sense of `IsSubvector`.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2);

// Creates bookmark path for `folderId` passed in. For eg: for folderId = 76,
// Root node(0) --> MobileBookmarks (3) --> Test1(76) will be returned as [0, 3,
// 76].
NSArray<NSNumber*>* CreateBookmarkPath(bookmarks::BookmarkModel* model,
                                       int64_t folder_id);

// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString);

// Uses `IsBookmarked` to check whether `url` is bookmarked in any of the
// provided bookmark models. `account_model` can be null.
bool IsBookmarked(const GURL& url,
                  bookmarks::BookmarkModel* local_model,
                  bookmarks::BookmarkModel* account_model);

// Uses `GetMostRecentlyAddedUserNodeForURL` to find the most recently added
// bookmark node with the corresponding URL in both models. If both models
// contain matching entries - compares them and returns the most recently added
// entry. If only one model has a matching entry - returns that entry. If no
// models contain matching entries - returns null. `account_model` can be null.
const bookmarks::BookmarkNode* GetMostRecentlyAddedUserNodeForURL(
    const GURL& url,
    bookmarks::BookmarkModel* local_model,
    bookmarks::BookmarkModel* account_model);

}  // namespace bookmark_utils_ios

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UTILS_IOS_H_
