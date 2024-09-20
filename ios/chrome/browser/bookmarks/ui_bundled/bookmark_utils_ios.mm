// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <stdint.h>

#import <algorithm>
#import <iterator>
#import <memory>
#import <vector>

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/containers/flat_map.h"
#import "base/hash/hash.h"
#import "base/i18n/message_formatter.h"
#import "base/i18n/string_compare.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/query_parser/query_parser.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/undo_manager_wrapper.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/models/tree_node_iterator.h"

using bookmarks::BookmarkNode;

namespace bookmark_utils_ios {

NSString* const kBookmarksSnackbarCategory = @"BookmarksSnackbarCategory";

namespace {

void RemoveBookmarksRecursive(const std::set<const BookmarkNode*>& bookmarks,
                              bookmarks::BookmarkModel* model,
                              bookmarks::metrics::BookmarkEditSource source,
                              const BookmarkNode* node,
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

const BookmarkNode* FindFolderById(const bookmarks::BookmarkModel* model,
                                   int64_t id) {
  CHECK(model);
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);
  return (node && node->is_folder()) ? node : nullptr;
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

#pragma mark - Profile and account

BookmarkStorageType GetBookmarkStorageType(
    const BookmarkNode* bookmark_node,
    const bookmarks::BookmarkModel* bookmark_model) {
  DCHECK(bookmark_node);
  DCHECK(bookmark_model);
  return bookmark_model->IsLocalOnlyNode(*bookmark_node)
             ? BookmarkStorageType::kLocalOrSyncable
             : BookmarkStorageType::kAccount;
}

bool IsAccountBookmarkStorageOptedIn(syncer::SyncService* sync_service) {
  if (sync_service->GetAccountInfo().IsEmpty()) {
    return false;
  }
  // TODO(crbug.com/40066949): Remove this after UNO phase 3. See
  // ConsentLevel::kSync documentation for more details.
  if (sync_service->HasSyncConsent()) {
    return false;
  }
  syncer::UserSelectableTypeSet selected_types =
      sync_service->GetUserSettings()->GetSelectedTypes();
  return selected_types.Has(syncer::UserSelectableType::kBookmarks);
}

bool IsAccountBookmarkStorageAvailable(const bookmarks::BookmarkModel* model) {
  CHECK(model);
  return model->account_mobile_node() != nullptr;
}

#pragma mark - Updating Bookmarks

// Creates a toast which will undo the changes made to the bookmark model if
// the user presses the undo button, and the UndoManagerWrapper allows the undo
// to go through.
MDCSnackbarMessage* CreateUndoToastWithWrapper(UndoManagerWrapper* wrapper,
                                               NSString* text,
                                               std::string user_action) {
  DCHECK(!user_action.empty());
  // Create the block that will be executed if the user taps the undo button.
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = ^{
    if (![wrapper hasUndoManagerChanged]) {
      base::RecordAction(base::UserMetricsAction(user_action.c_str()));
      [wrapper undo];
    }
  };

  action.title = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_UNDO_BUTTON_TITLE);
  action.accessibilityIdentifier = @"Undo";
  action.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_UNDO_BUTTON_TITLE);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = CreateSnackbarMessage(text);
  message.action = action;
  message.category = kBookmarksSnackbarCategory;
  return message;
}

bool UpdateBookmark(const BookmarkNode* node,
                    NSString* title,
                    const GURL& url,
                    const BookmarkNode* folder,
                    bookmarks::BookmarkModel* model) {
  DCHECK(node);
  DCHECK(folder);
  std::u16string titleString = base::SysNSStringToUTF16(title);
  if (node->GetTitle() == titleString && node->url() == url &&
      node->parent() == folder) {
    // Nothing to do.
    return false;
  }

  model->SetTitle(node, titleString,
                  bookmarks::metrics::BookmarkEditSource::kUser);
  model->SetURL(node, url, bookmarks::metrics::BookmarkEditSource::kUser);

  DCHECK(!folder->HasAncestor(node));
  if (node->parent() != folder) {
    model->Move(node, folder, folder->children().size());
  }
  return true;
}

bool bookmarkSavedIntoAccount(
    BookmarkStorageType bookmarkStorageType,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService) {
  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  BOOL hasSyncConsent =
      authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSync);
  BOOL savedIntoAccount =
      (bookmarkStorageType == BookmarkStorageType::kAccount) ||
      (hasSyncConsent && syncService->GetUserSettings()->GetSelectedTypes().Has(
                             syncer::UserSelectableType::kBookmarks));
  return savedIntoAccount;
}

NSString* messageForAddingBookmarksInFolder(
    const BookmarkNode* folder,
    const bookmarks::BookmarkModel* model,
    bool chosenByUser,
    bool showCount,
    int count,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService) {
  CHECK(folder);
  CHECK(model);

  NSString* folderTitle = TitleForBookmarkNode(folder);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  if (!identity ||
      !syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kBookmarks) ||
      !IsAccountBookmarkStorageAvailable(model)) {
    // The user is signed-out, bookmark sync is disabled, or the account
    // bookmark is not available (e.g. the account is passphrase protected and
    // the passphrase was not entered).
    if (chosenByUser) {
      std::u16string title = base::SysNSStringToUTF16(folderTitle);
      std::u16string pattern = l10n_util::GetStringUTF16(
          (showCount) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_FOLDER
                      : IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
      return base::SysUTF16ToNSString(
          base::i18n::MessageFormatter::FormatWithNamedArgs(
              pattern, "count", count, "title", title));
    } else {
      return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          (showCount) ? IDS_IOS_BOOKMARKS_BULK_SAVED : IDS_IOS_BOOKMARKS_SAVED,
          count));
    }
  }

  // The user is signed in and bookmark sync is on (either account bookmarks or
  // the legacy sync-the-feature). It is still possible that the folder saving
  // into is a local-only folder.
  if (model->IsLocalOnlyNode(*folder)) {
    std::u16string title = base::SysNSStringToUTF16(folderTitle);
    std::u16string pattern = l10n_util::GetStringUTF16(
        (showCount) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_FOLDER_TO_DEVICE
                    : IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER_TO_DEVICE);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title", title);
    return base::SysUTF16ToNSString(message);
  }

  // Tell the user the bookmark is synced in their account.
  std::u16string email = base::SysNSStringToUTF16(identity.userEmail);
  if (chosenByUser) {
    // Also mentions the folder in which the bookmark is saved.
    std::u16string title = base::SysNSStringToUTF16(folderTitle);
    std::u16string pattern = l10n_util::GetStringUTF16(
        (showCount) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_INTO_ACCOUNT_FOLDER
                    : IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT_FOLDER);
    return base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "count", count, "title", title, "email", email));
  } else {
    std::u16string pattern = l10n_util::GetStringUTF16(
        (showCount) ? IDS_IOS_BOOKMARK_PAGE_BULK_SAVED_INTO_ACCOUNT
                    : IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT);
    return base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "count", count, "email", email));
  }
}

MDCSnackbarMessage* UpdateBookmarkWithUndoToast(
    const BookmarkNode* node,
    NSString* title,
    const GURL& url,
    const BookmarkNode* original_folder,
    const BookmarkNode* folder,
    bookmarks::BookmarkModel* model,
    ProfileIOS* profile,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService) {
  CHECK(node);

  // Secondly, create an Undo group for all undoable actions.
  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:profile];

  // Create or update the bookmark.
  [wrapper startGroupingActions];

  bool did_change_anything = UpdateBookmark(node, title, url, folder, model);

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  if (!did_change_anything) {
    return nil;
  }
  NSString* text = nil;
  if (original_folder == folder) {
    text = l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARK_UPDATED);
  } else {
    text = messageForAddingBookmarksInFolder(
        folder, model,
        /*chosenByUser =*/true,
        /*showCount=*/false, /*count=*/1, authenticationService, syncService);
  }
  return CreateUndoToastWithWrapper(
      wrapper, text, "MobileBookmarkManagerUpdatedBookmarkUndone");
}

MDCSnackbarMessage* CreateBookmarkAtPositionWithUndoToast(
    NSString* title,
    const GURL& url,
    const BookmarkNode* folder,
    int position,
    bookmarks::BookmarkModel* model,
    ProfileIOS* profile) {
  std::u16string titleString = base::SysNSStringToUTF16(title);

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:profile];
  [wrapper startGroupingActions];

  RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts);
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  const BookmarkNode* node =
      model->AddNewURL(folder, folder->children().size(), titleString, url);
  model->Move(node, folder, position);

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARK_CREATED);
  return CreateUndoToastWithWrapper(wrapper, text,
                                    "MobileBookmarkManagerAddedBookmarkUndone");
}

MDCSnackbarMessage* UpdateBookmarkPositionWithUndoToast(
    const BookmarkNode* node,
    const BookmarkNode* folder,
    size_t position,
    bookmarks::BookmarkModel* model,
    ProfileIOS* profile) {
  DCHECK(node);
  DCHECK(folder);
  DCHECK(!folder->HasAncestor(node));

  size_t old_index = node->parent()->GetIndexOf(node).value();
  // Early return if no change in position.
  if (node->parent() == folder && old_index == position) {
    return nil;
  }

  // Secondly, create an Undo group for all undoable actions.
  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:profile];

  // Update the bookmark.
  [wrapper startGroupingActions];

  model->Move(node, folder, position);

  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  NSString* text =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_NEW_BOOKMARK_UPDATED);
  return CreateUndoToastWithWrapper(wrapper, text,
                                    "MobileBookmarkManagerMoveToFolderUndone");
}

void DeleteBookmarks(const std::set<const BookmarkNode*>& bookmarks,
                     bookmarks::BookmarkModel* bookmark_model,
                     const base::Location& location) {
  CHECK(bookmark_model);
  CHECK(bookmark_model->loaded());
  RemoveBookmarksRecursive(bookmarks, bookmark_model,
                           bookmarks::metrics::BookmarkEditSource::kUser,
                           bookmark_model->root_node(), location);
}

MDCSnackbarMessage* DeleteBookmarksWithUndoToast(
    const std::set<const BookmarkNode*>& nodes,
    bookmarks::BookmarkModel* bookmark_model,
    ProfileIOS* profile,
    const base::Location& location) {
  size_t node_count = nodes.size();
  DCHECK_GT(node_count, 0u);

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:profile];

  // Delete the selected bookmarks.
  [wrapper startGroupingActions];
  bookmark_utils_ios::DeleteBookmarks(nodes, bookmark_model, location);
  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  return CreateUndoToastWithWrapper(
      wrapper,
      l10n_util::GetPluralNSStringF(IDS_IOS_BOOKMARK_DELETED_BOOKMARKS,
                                    node_count),
      "MobileBookmarkManagerDeletedEntryUndone");
}

using BookmarkNodeVectorIterator = std::vector<const BookmarkNode*>::iterator;

bool MoveBookmarks(const std::vector<const BookmarkNode*>& bookmarks_to_move,
                   bookmarks::BookmarkModel* model,
                   const BookmarkNode* destination_folder) {
  bool did_perform_move = false;

  for (const BookmarkNode* node : bookmarks_to_move) {
    if (destination_folder->HasAncestor(node)) {
      continue;  // Cannot move a folder into one of its descendants.
    }

    if (node->parent() == destination_folder) {
      continue;  // Already in `destination_folder`, nothing to do.
    }

    model->Move(node, destination_folder,
                destination_folder->children().size());
    did_perform_move = true;
  }

  return did_perform_move;
}

MDCSnackbarMessage* MoveBookmarksWithUndoToast(
    const std::vector<const BookmarkNode*>& bookmarks_to_move,
    bookmarks::BookmarkModel* model,
    const BookmarkNode* destination_folder,
    ProfileIOS* profile,
    base::WeakPtr<AuthenticationService> authenticationService,
    raw_ptr<syncer::SyncService> syncService) {
  size_t node_count = bookmarks_to_move.size();
  DCHECK_GT(node_count, 0u);
  bool contains_a_folder =
      std::find_if(bookmarks_to_move.begin(), bookmarks_to_move.end(),
                   [](const BookmarkNode* node) {
                     return node->is_folder();
                   }) != bookmarks_to_move.end();
  // We assume a folder contains multiple bookmark, and thus plural can be used.
  bool multiple_bookmarks_to_move = node_count > 1 || contains_a_folder;

  UndoManagerWrapper* wrapper =
      [[UndoManagerWrapper alloc] initWithBrowserState:profile];

  // Move the selected bookmarks.
  [wrapper startGroupingActions];
  const bool did_perform_move =
      MoveBookmarks(bookmarks_to_move, model, destination_folder);
  [wrapper stopGroupingActions];
  [wrapper resetUndoManagerChanged];

  if (!did_perform_move) {
    return nil;  // Don't return a snackbar when no real move as happened.
  }

  // As the exact number of bookmarks is not displayed, assuming there are two
  // is fine for the purpose of the bookmark. What matters is that it leads to
  // "bookmarks" in plural form.
  int count = (multiple_bookmarks_to_move) ? 2 : 1;

  NSString* text = messageForAddingBookmarksInFolder(
      destination_folder, model,
      /*chosenByUser=*/true, /*showCount=*/false, count, authenticationService,
      syncService);
  return CreateUndoToastWithWrapper(wrapper, text,
                                    "MobileBookmarkManagerMoveToFolderUndone");
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
  raw_ptr<icu::Collator> collator_;
};

bool FolderHasAncestorInBookmarkNodes(const BookmarkNode* folder,
                                      const NodeSet& bookmarkNodes) {
  DCHECK(folder);
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
                                     const bookmarks::BookmarkModel* model,
                                     BookmarkStorageType type) {
  NodeVector primary_nodes = PrimaryPermanentNodes(model, type);
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

NSArray<NSNumber*>* CreateBookmarkPath(const bookmarks::BookmarkModel* model,
                                       int64_t folder_id) {
  const BookmarkNode* bookmark = FindFolderById(model, folder_id);
  if (!bookmark || bookmark->is_root()) {
    return nil;
  }

  NSMutableArray<NSNumber*>* bookmarkPath = [NSMutableArray array];
  while (!bookmark->is_root()) {
    [bookmarkPath addObject:[NSNumber numberWithLongLong:bookmark->id()]];
    bookmark = bookmark->parent();
    DCHECK(bookmark);
  }
  return [[bookmarkPath reverseObjectEnumerator] allObjects];
}

GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}

}  // namespace bookmark_utils_ios
