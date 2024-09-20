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
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class AuthenticationService;
enum class BookmarkStorageType;
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

typedef std::vector<const bookmarks::BookmarkNode*> NodeVector;
typedef std::set<const bookmarks::BookmarkNode*> NodeSet;

// Finds bookmark node passed in `id`, in the `model`. Returns null if the
// node is found but not a folder.
const bookmarks::BookmarkNode* FindFolderById(
    const bookmarks::BookmarkModel* model,
    int64_t id);

// The iOS code is doing some munging of the bookmark folder names in order
// to display a slighly different wording for the default folders.
NSString* TitleForBookmarkNode(const bookmarks::BookmarkNode* node);

// Returns the storage type for a node, based on where `bookmark_node` exists
// in `bookmark_model`, i.e. under which permanent folder it exists.
// `bookmark_node` is the bookmark to query. It must not be null.
// `bookmark_model` is bookmark model. It must not be null. `bookmark_model`
// must contain `bookmark_node`. This function is linear in time in the depth of
// the bookmark_node.
BookmarkStorageType GetBookmarkStorageType(
    const bookmarks::BookmarkNode* bookmark_node,
    const bookmarks::BookmarkModel* bookmark_model);

// Returns true if the user is signed in and they opted in for the account
// bookmark storage.
bool IsAccountBookmarkStorageOptedIn(syncer::SyncService* sync_service);

// Returns true if the user opted-in and can use the account storage.
// E.g. if the passphrase is missing, the storage may not be available.
// `model` must not be null.
bool IsAccountBookmarkStorageAvailable(const bookmarks::BookmarkModel* model);

// Updates `node`.
// `folder` is the intended parent of `node`.
// Returns a boolean signifying whether any change was performed.
bool UpdateBookmark(const bookmarks::BookmarkNode* node,
                    NSString* title,
                    const GURL& url,
                    const bookmarks::BookmarkNode* folder,
                    bookmarks::BookmarkModel* model);

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
    ProfileIOS* profile,
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
    ProfileIOS* profile);

// Updates a bookmark node position, and returns a snackbar with an undo action.
// Returns nil if the operation wasn't successful or there's nothing to undo.
MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder,
    size_t position,
    bookmarks::BookmarkModel* model,
    ProfileIOS* profile);

// Deletes all nodes in `bookmarks` from `bookmark_model` and returns a snackbar
// with an undo action. Returns nil if the operation wasn't successful or
// there's nothing to undo.
MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const bookmarks::BookmarkNode*>& bookmarks,
    bookmarks::BookmarkModel* bookmark_model,
    ProfileIOS* profile,
    const base::Location& location);

// Deletes all nodes in `bookmarks`.
void DeleteBookmarks(const std::set<const bookmarks::BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* bookmark_model,
                     const base::Location& location);

// Move all `bookmarks_to_move` to the given `folder`, and returns a snackbar
// with an undo action. Returns nil if the operation wasn't successful or
// there's nothing to undo.
MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* destination_folder,
    ProfileIOS* profile,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// Move all `bookmarks` to the given `folder`.
// Returns whether this method actually moved bookmarks (for example, only
// moving a folder to its parent will return `false`).
bool MoveBookmarks(
    const std::vector<const bookmarks::BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* model,
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
                                     BookmarkStorageType type);

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
NSArray<NSNumber*>* CreateBookmarkPath(const bookmarks::BookmarkModel* model,
                                       int64_t folder_id);

// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString);

// The localized strings for adding bookmarks.
// `folder`:  The folder into which bookmarks were added. Must not be nil.
// `model` must not be null.
// `chosenByUser`: whether this is the last folder in which the user moved a
// bookmark since last time the set of model changed.
// `showCount`: Display the number of moved bookmarks in the snackbar.
// `count`: the number of bookmarks.
NSString* messageForAddingBookmarksInFolder(
    const bookmarks::BookmarkNode* folder,
    const bookmarks::BookmarkModel* model,
    bool chosenByUser,
    bool showCount,
    int count,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

// The bookmark is saved in the account if either following condition is true:
// * the saved folder is an account bookmark,
// * the sync consent has been granted and the bookmark data type is enabled
bool bookmarkSavedIntoAccount(
    BookmarkStorageType bookmarkStorageType,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService);

}  // namespace bookmark_utils_ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_BOOKMARK_UTILS_IOS_H_
