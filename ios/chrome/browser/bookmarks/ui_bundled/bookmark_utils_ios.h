// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UTILS_IOS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UTILS_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <set>
#import <string>
#import <vector>

#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "base/uuid.h"

class AuthenticationService;
enum class BookmarkModelType;
class ChromeBrowserState;
class GURL;
@class MDCSnackbarMessage;
class LegacyBookmarkModel;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}  // namespace syncer

namespace bookmark_utils_ios {

// This class holds a node id.
struct BookmarkNodeReference {
  explicit BookmarkNodeReference(int64_t node_id);
  BookmarkNodeReference(const BookmarkNodeReference&);
  ~BookmarkNodeReference();

  // This operator is needed to be used `BookmarkNodeReference` in a std::set.
  bool operator<(const BookmarkNodeReference reference) const;
  BookmarkNodeReference& operator=(const BookmarkNodeReference& other) = delete;

  // Node id for the BookmarkNode.
  int64_t node_id;
};

typedef std::vector<const bookmarks::BookmarkNode*> NodeVector;
typedef std::set<const bookmarks::BookmarkNode*> NodeSet;
typedef std::set<BookmarkNodeReference> NodeReferenceSet;

// Converts a set of BookmarkNode into a set of BookmarkNodeReference.
NodeReferenceSet FindNodeReferenceByNodes(const NodeSet& nodes);

// Converts a BookmarkNodeReference into a BookmarkNode. This function might
// returns `nullptr` if the bookmark node doesn't exist anymore. `model` must
// not be null.
const bookmarks::BookmarkNode* FindNodeByNodeReference(
    const bookmarks::BookmarkModel* model,
    BookmarkNodeReference reference);

// Converts a set of BookmarkNodeReference into a set of BookmarkNode. This
// function might return fewer BookmarkNodeReference objects than BookmarkNode
// if the nodes don't exist anymore. `model` must not be null.
NodeSet FindNodesByNodeReferences(const bookmarks::BookmarkModel* model,
                                  const NodeReferenceSet& references);

// Finds bookmark node passed in `id`, in the `model`.
const bookmarks::BookmarkNode* FindNodeById(LegacyBookmarkModel* model,
                                            int64_t id);

// Finds bookmark node passed in `id`, in the `model`. Returns null if the
// node is found but not a folder.
const bookmarks::BookmarkNode* FindFolderById(LegacyBookmarkModel* model,
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
BookmarkModelType GetBookmarkModelType(
    const bookmarks::BookmarkNode* bookmark_node,
    LegacyBookmarkModel* profile_model,
    LegacyBookmarkModel* account_model);

// Returns the model type for a node, based on where `bookmark_node` exists in
// `bookmar_model`, i.e. under which permanent folder it exists.
// `bookmark_node` is the bookmark to query. It must not be null.
// `bookmark_model` is bookmark model. It must not be null.
// `bookmark_model` must contain `bookmark_node`.
// This function is linear in time in the depth of the bookmark_node.
BookmarkModelType GetBookmarkModelType(
    const bookmarks::BookmarkNode* bookmark_node,
    const bookmarks::BookmarkModel* bookmark_model);

// Returns the bookmark model for a node, based on profile model and account
// model.
// `bookmark_node` is the bookmark to query. It can not be null.
// `profile_model` is the profile mode. It can not be null.
// `account_model` is the account mode. It can be null.
// The node must belong to one of the two models.
// This function is linear in time in the depth of the bookmark_node because it
// uses `GetBookmarkModelType(...)`.
LegacyBookmarkModel* GetBookmarkModelForNode(
    const bookmarks::BookmarkNode* bookmark_node,
    LegacyBookmarkModel* profile_model,
    LegacyBookmarkModel* account_model);

// Returns true if the user is signed in and they opted in for the account
// bookmark storage.
bool IsAccountBookmarkStorageOptedIn(syncer::SyncService* sync_service);

// Returns true if the user opted-in and can use the account storage.
// E.g. if the passphrase is missing, the storage may not be available.
// `model` must not be null.
bool IsAccountBookmarkStorageAvailable(const bookmarks::BookmarkModel* model);

// Legacy equivalent of the above.
bool IsAccountBookmarkStorageAvailable(syncer::SyncService* sync_service,
                                       LegacyBookmarkModel* account_model);

// Updates `node`.
// `folder` is the intended parent of `node`.
// Returns a boolean signifying whether any change was performed.
bool UpdateBookmark(const bookmarks::BookmarkNode* node,
                    NSString* title,
                    const GURL& url,
                    const bookmarks::BookmarkNode* folder,
                    bookmarks::BookmarkModel* model);

// Legacy equivalent for the above.
bool UpdateBookmark(const bookmarks::BookmarkNode* node,
                    NSString* title,
                    const GURL& url,
                    const bookmarks::BookmarkNode* folder,
                    LegacyBookmarkModel* local_or_syncable_model,
                    LegacyBookmarkModel* account_model);

// Similar to `UpdateBookmark`, but returns a snackbar that allows to
// undo the performed action. Returns nil if there's nothing to undo.
// TODO(crbug.com/40137712): Refactor to include position and replace two
// functions below.
MDCSnackbarMessage* UpdateBookmarkWithUndoToast(
    const bookmarks::BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* original_folder,
    const bookmarks::BookmarkNode* folder,
    bookmarks::BookmarkModel* model,
    ChromeBrowserState* browser_state,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// Legacy equivalent for the above.
MDCSnackbarMessage* UpdateBookmarkWithUndoToast(
    const bookmarks::BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* original_folder,
    const bookmarks::BookmarkNode* folder,
    LegacyBookmarkModel* local_or_syncable_model,
    LegacyBookmarkModel* account_model,
    ChromeBrowserState* browser_state,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// Creates a new bookmark with `title`, `url`, at `position` under parent
// `folder`. Returns a snackbar with an undo action. Returns nil if operation
// failed or there's nothing to undo.
MDCSnackbarMessage* CreateBookmarkAtPositionWithUndoToast(
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    int position,
    bookmarks::BookmarkModel* model,
    ChromeBrowserState* browser_state);

// Legacy equivalent for the above.
MDCSnackbarMessage* CreateBookmarkAtPositionWithUndoToast(
    NSString* title,
    const GURL& url,
    const bookmarks::BookmarkNode* folder,
    int position,
    LegacyBookmarkModel* local_or_syncable_model,
    LegacyBookmarkModel* account_model,
    ChromeBrowserState* browser_state);

// Updates a bookmark node position, and returns a snackbar with an undo action.
// Returns nil if the operation wasn't successful or there's nothing to undo.
MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    size_t position,
    bookmarks::BookmarkModel* model,
    ChromeBrowserState* browser_state);

// Legacy equivalent for the above. `node` and `folder` must belong to the same
// model.
MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    size_t position,
    LegacyBookmarkModel* local_or_syncable_model,
    LegacyBookmarkModel* account_model,
    ChromeBrowserState* browser_state);

// Deletes all nodes in `bookmarks` from `bookmark_model` and returns a snackbar
// with an undo action. Returns nil if the operation wasn't successful or
// there's nothing to undo.
MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* bookmark_model,
    ChromeBrowserState* browser_state,
    const base::Location& location);

// Legacy equivalent for the above.
MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    const std::vector<LegacyBookmarkModel*>& bookmark_models,
    ChromeBrowserState* browser_state,
    const base::Location& location);

// Deletes all nodes in `bookmarks`.
void DeleteBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* bookmark_model,
                     const base::Location& location);

// Legacy equivalent for the above.
void DeleteBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                     LegacyBookmarkModel* model,
                     const base::Location& location);

// Move all `bookmarks_to_move` to the given `folder`, and returns a snackbar
// with an undo action. Returns nil if the operation wasn't successful or
// there's nothing to undo.
MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* destination_folder,
    ChromeBrowserState* browser_state,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// Legacy equivalent for the above.
// This method updates `bookmarks_to_move` with new pointers to moved nodes, see
// `MoveBookmarks` documentation for details.
MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    LegacyBookmarkModel* local_or_syncable_model,
    LegacyBookmarkModel* account_model,
    const bookmarks::BookmarkNode* destination_folder,
    ChromeBrowserState* browser_state,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// Move all `bookmarks` to the given `folder`.
// Returns whether this method actually moved bookmarks (for example, only
// moving a folder to its parent will return `false`).
bool MoveBookmarks(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* destination_folder);

// Legacy equivalent for the above.
// This method updates `bookmarks_to_move` with new pointers to moved nodes. In
// other words, when the node contained in `bookmarks_to_move` at index N is
// moved - the updated `BookmarkNode` pointer is saved in `bookmarks_to_move` at
// the same index N.
bool MoveBookmarks(
    std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    LegacyBookmarkModel* local_model,
    LegacyBookmarkModel* account_model,
    const bookmarks::BookmarkNode* destination_folder);

// Category name for all bookmarks related snackbars.
extern NSString* const kBookmarksSnackbarCategory;

// Sorts a vector full of folders by title.
void SortFolders(NodeVector* vector);

// Returns a vector of top-level permanent folders as filtered by `type` and
// all their descendant folders, except for those included in `obstructions`
// which are excluded, as well as their descendants. The returned list is
// sorted depth-first, then alphabetically.
NodeVector VisibleNonDescendantNodes(const NodeSet& obstructions,
                                     const bookmarks::BookmarkModel* model,
                                     BookmarkModelType type);

// Whether `vector1` contains only elements of `vector2` in the same order.
BOOL IsSubvectorOfNodes(const NodeVector& vector1, const NodeVector& vector2);

// Returns the indices in `vector2` of the items in `vector2` that are not
// present in `vector1`.
// `vector1` MUST be a subvector of `vector2` in the sense of `IsSubvector`.
std::vector<NodeVector::size_type> MissingNodesIndices(
    const NodeVector& vector1,
    const NodeVector& vector2);

// Creates bookmark path for `folderId` passed in. For eg: for folderId = 76,
// MobileBookmarks (3) --> Test1(76) will be returned as [3, 76], where the
// first element always represents a permanent folder. Returns nullptr if the
// folder is not found or is the root node.
NSArray<NSNumber*>* CreateBookmarkPath(LegacyBookmarkModel* model,
                                       int64_t folder_id);

// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString);

// Uses `GetMostRecentlyAddedUserNodeForURL` to find the most recently added
// bookmark node with the corresponding URL in both models. If both models
// contain matching entries - compares them and returns the most recently added
// entry. If only one model has a matching entry - returns that entry. If no
// models contain matching entries - returns null. `account_model` can be null.
const bookmarks::BookmarkNode* GetMostRecentlyAddedUserNodeForURL(
    const GURL& url,
    LegacyBookmarkModel* local_model,
    LegacyBookmarkModel* account_model);

// The localized strings for adding bookmarks.
// `folderTitle`:  The name of the folder. Assumed to be non-nil.
// `chosenByUser`: whether this is the last folder in which the user moved a
// bookmark since last time the set of model changed.
// `bookmarkModelType`: whether it  is is on account storage, or local or
// syncable. `showCount`: Display the number of moved bookmarks in the snackbar.
// `count`: the number of bookmarks.
NSString* messageForAddingBookmarksInFolder(
    NSString* folderTitle,
    bool chosenByUser,
    BookmarkModelType bookmarkModelType,
    bool showCount,
    int count,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// The bookmark is saved in the account if either following condition is true:
// * the saved folder is in the account model,
// * the sync consent has been granted and the bookmark data type is enabled
bool bookmarkSavedIntoAccount(
    BookmarkModelType bookmarkModelType,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

}  // namespace bookmark_utils_ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UTILS_IOS_H_
