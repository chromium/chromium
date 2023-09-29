// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/web_state_list_serialization.h"

#import <stdint.h>

#import <algorithm>
#import <memory>
#import <unordered_map>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Some WebState may have no back/forward history. This can happen for
// multiple reason (one is when opening a new tab on a slow network session,
// and terminating the app before the navigation can commit, another is when
// WKWebView intercepts a new tab navigation to an app navigation; there may
// be other cases). This function creates a RemovingIndexes that records the
// indexes of the WebStates that should not be saved.
RemovingIndexes GetIndexOfWebStatesToDrop(const WebStateList& web_state_list) {
  std::vector<int> web_state_to_skip_indexes;
  for (int index = 0; index < web_state_list.count(); ++index) {
    web::WebState* web_state = web_state_list.GetWebStateAt(index);
    bool in_restore =
        web_state->IsRealized() && web_state->GetNavigationManager() &&
        web_state->GetNavigationManager()->IsRestoreSessionInProgress();
    if (!in_restore && web_state->GetNavigationItemCount() == 0) {
      web_state_to_skip_indexes.push_back(index);
    }
  }
  return RemovingIndexes(std::move(web_state_to_skip_indexes));
}

// Represents the reference to a WebState's opener in a WebStateList
// using indexes instead of a pointer to the opener WebState.
struct OpenerReference {
  const int index;
  const int navigation_index;

  // Use when the WebState has not opener.
  static OpenerReference Invalid() {
    return OpenerReference{.index = -1, .navigation_index = -1};
  }
};

// Class abstracting getting required information from the serialized
// WebStateList representation when restoring the state. This is used
// to abstract the difference between restoring from the legacy and
// optimized storage.
class Deserializer {
 public:
  Deserializer() = default;
  virtual ~Deserializer() = default;

  // Returns the index of the active tab according to the serialized state.
  virtual int GetActiveIndex() const = 0;

  // Returns the total number of tabs in the serialized state.
  virtual int GetRestoredTabsCount() const = 0;

  // Returns the number of tabs that are considered as pinned according to
  // the serialized state. They must come before all unpinned tabs. May be
  // ignored if asked to restore the tabs without support of pinned tabs.
  virtual int GetRestoredPinnedTabsCount() const = 0;

  // Returns the reference to the opener of tab at `index`. If it has no
  // opener, must return OpenerReference::Invalid().
  virtual OpenerReference GetOpenerForTabAt(int index) const = 0;

  // Creates and return the WebState at `index`.
  virtual std::unique_ptr<web::WebState> RestoreTabAt(int index) const = 0;
};

// An implementation of Deserializer used to restore from legacy storage.
class DeserializeFromSessionWindow : public Deserializer {
 public:
  DeserializeFromSessionWindow(SessionWindowIOS* session_window,
                               const WebStateFactory& factory);

  // Deserializer implementation.
  int GetActiveIndex() const override;
  int GetRestoredTabsCount() const override;
  int GetRestoredPinnedTabsCount() const override;
  OpenerReference GetOpenerForTabAt(int index) const override;
  std::unique_ptr<web::WebState> RestoreTabAt(int index) const override;

 private:
  SessionWindowIOS* const session_window_;
  WebStateFactory const factory_;
};

DeserializeFromSessionWindow::DeserializeFromSessionWindow(
    SessionWindowIOS* session_window,
    const WebStateFactory& factory)
    : session_window_(session_window), factory_(factory) {
  DCHECK(factory_);
}

int DeserializeFromSessionWindow::GetActiveIndex() const {
  if (!session_window_ || session_window_.selectedIndex == NSNotFound) {
    return -1;
  }
  return static_cast<int>(session_window_.selectedIndex);
}

int DeserializeFromSessionWindow::GetRestoredTabsCount() const {
  return session_window_.sessions.count;
}

int DeserializeFromSessionWindow::GetRestoredPinnedTabsCount() const {
  int pinned_tabs_count = 0;
  for (CRWSessionStorage* session in session_window_.sessions) {
    CRWSessionUserData* user_data = session.userData;
    NSNumber* pinned_state = base::apple::ObjCCast<NSNumber>(
        [user_data objectForKey:kLegacyWebStateListPinnedStateKey]);
    if (!pinned_state || ![pinned_state boolValue]) {
      // The pinned tabs are always at the beginning of the list,
      // so stop iterating as soon as a non-pinned tab is found.
      break;
    }

    ++pinned_tabs_count;
  }
  return pinned_tabs_count;
}

OpenerReference DeserializeFromSessionWindow::GetOpenerForTabAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(session_window_.sessions.count));
  CRWSessionStorage* session = session_window_.sessions[index];
  CRWSessionUserData* user_data = session.userData;

  NSNumber* opener_index = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);

  NSNumber* opener_navigation_index = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListOpenerNavigationIndexKey]);

  if (!opener_index || !opener_navigation_index) {
    return OpenerReference::Invalid();
  }

  return OpenerReference{
      .index = [opener_index intValue],
      .navigation_index = [opener_navigation_index intValue],
  };
}

std::unique_ptr<web::WebState> DeserializeFromSessionWindow::RestoreTabAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(session_window_.sessions.count));
  return factory_.Run(session_window_.sessions[index]);
}

// An implementation of Deserializer used to restore from optimized storage.
class DeserializeFromProto : public Deserializer {
 public:
  DeserializeFromProto(ios::proto::WebStateListStorage storage,
                       const WebStateFactoryFromProto& factory);

  // Deserializer implementation.
  int GetActiveIndex() const override;
  int GetRestoredTabsCount() const override;
  int GetRestoredPinnedTabsCount() const override;
  OpenerReference GetOpenerForTabAt(int index) const override;
  std::unique_ptr<web::WebState> RestoreTabAt(int index) const override;

 private:
  ios::proto::WebStateListStorage const storage_;
  WebStateFactoryFromProto const factory_;
};

DeserializeFromProto::DeserializeFromProto(
    ios::proto::WebStateListStorage storage,
    const WebStateFactoryFromProto& factory)
    : storage_(std::move(storage)), factory_(factory) {
  DCHECK(factory_);
}

int DeserializeFromProto::GetActiveIndex() const {
  return storage_.active_index();
}

int DeserializeFromProto::GetRestoredTabsCount() const {
  return storage_.items_size();
}

int DeserializeFromProto::GetRestoredPinnedTabsCount() const {
  return storage_.pinned_item_count();
}

OpenerReference DeserializeFromProto::GetOpenerForTabAt(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.items_size());
  const auto& item_storage = storage_.items(index);
  if (!item_storage.has_opener()) {
    return OpenerReference::Invalid();
  }

  const auto& opener_storage = item_storage.opener();
  return OpenerReference{
      .index = opener_storage.index(),
      .navigation_index = opener_storage.navigation_index(),
  };
}

std::unique_ptr<web::WebState> DeserializeFromProto::RestoreTabAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, storage_.items_size());
  const auto& item_storage = storage_.items(index);
  return factory_.Run(
      web::WebStateID::FromSerializedValue(item_storage.identifier()));
}

// Used to store the range of tabs to restore.
struct DeserializationRange {
  const int min;
  const int max;

  // Returns the number of items in range.
  int length() const { return max - min; }

  // Returns whether the range contains any item.
  bool empty() const { return min >= max; }

  // Returns whether `index` is contained in the range.
  bool contains(int index) const { return min <= index && index < max; }

  // Returns a range for restoring tabs accoring to `scope`.
  static DeserializationRange Create(int tabs_count,
                                     int pinned_tabs_count,
                                     SessionRestorationScope scope);
};

DeserializationRange DeserializationRange::Create(
    int tabs_count,
    int pinned_tabs_count,
    SessionRestorationScope scope) {
  switch (scope) {
    case SessionRestorationScope::kAll:
      return DeserializationRange{.min = 0, .max = tabs_count};

    case SessionRestorationScope::kPinnedOnly:
      return DeserializationRange{.min = 0, .max = pinned_tabs_count};

    case SessionRestorationScope::kRegularOnly:
      return DeserializationRange{.min = pinned_tabs_count, .max = tabs_count};
  }

  NOTREACHED_NORETURN();
}

struct InsertionHelper {
  const int pinned_tabs_count;
  const int pinned_offset;
  const int regular_offset;

  int insertion_index(int index) const {
    if (index < pinned_tabs_count) {
      return pinned_offset + index;
    }
    return regular_offset + index;
  }

  int insertion_flags(int index, bool is_active) const {
    int flags = WebStateList::INSERT_FORCE_INDEX;
    if (index < pinned_tabs_count) {
      flags |= WebStateList::INSERT_PINNED;
    }
    if (is_active) {
      flags |= WebStateList::INSERT_ACTIVATE;
    }
    return flags;
  }
};

void DeserializeWebStateListInternal(
    SessionRestorationScope scope,
    bool enable_pinned_web_states,
    const Deserializer& deserializer,
    std::vector<web::WebState*>* restored_web_states,
    WebStateList* web_state_list) {
  DCHECK(web_state_list);
  DCHECK(restored_web_states);
  DCHECK(restored_web_states->empty());

  int restored_pinned_tabs_count = 0;
  const int restored_tabs_count = deserializer.GetRestoredTabsCount();
  if (enable_pinned_web_states) {
    // Ensure that restored_pinned_tabs_count is smaller than
    // restored_tabs_count (to avoid crashing if the storage
    // on disk is partially invalid).
    restored_pinned_tabs_count = std::min(
        restored_tabs_count, deserializer.GetRestoredPinnedTabsCount());
  }

  // If the restoration range is empty, then there is nothing to do. This
  // can happen if the storage is empty or if all items are out of `scope`.
  const DeserializationRange range = DeserializationRange::Create(
      restored_tabs_count, restored_pinned_tabs_count, scope);
  if (range.empty()) {
    return;
  }

  // If there is only one tab, and it is the new tab page, clobber it before
  // restoring any tabs (this avoid having to search for the tab amongst all
  // the ones that have been restored).
  if (web_state_list->count() == 1) {
    web::WebState* web_state = web_state_list->GetWebStateAt(0);

    // Check for realization before checking for a pending load to prevent
    // force-realization of the tab. An unrealized WebState cannot have a
    // load pending anyway.
    const bool has_pending_load =
        web_state->IsRealized() &&
        web_state->GetNavigationManager()->GetPendingItem();

    if (!has_pending_load) {
      const GURL last_committed_url = web_state->GetLastCommittedURL();
      if (last_committed_url == kChromeUINewTabURL) {
        web_state_list->CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);
      }
    }
  }

  // Instantiate an InsertionHelper object that will help compute the indexes
  // and flags used to insert the WebState in the WebStateList.
  //
  // The tabs will be inserted in order at the end of their section (pinned
  // or not), so the insertion index is the offset to the end of the section
  // plus their index in the stored. However, it is possible to ignore the
  // first `range.min` items when restoring, in which case the index needs
  // to be adjusted to compensate for that.
  const InsertionHelper helper{
      .pinned_tabs_count = restored_pinned_tabs_count,
      .pinned_offset = web_state_list->pinned_tabs_count() - range.min,
      .regular_offset = web_state_list->count() - range.min,
  };

  // Get the index of the active item according to storage. If it is in
  // the restoration scope. Used to mark the WebState as active during
  // the insertion, if in scope.
  const int active_index = deserializer.GetActiveIndex();

  // Restore all items in scope directly at their correct position in the
  // WebStateList. The opener-opened relationship is not restored yet, as
  // some WebState may have an opener that is stored after them.
  for (int index = range.min; index < range.max; ++index) {
    std::unique_ptr<web::WebState> web_state = deserializer.RestoreTabAt(index);
    restored_web_states->push_back(web_state.get());  // Store pointer to item.

    const int inserted_index = web_state_list->InsertWebState(
        helper.insertion_index(index), std::move(web_state),
        helper.insertion_flags(index, index == active_index), WebStateOpener{});

    DCHECK_EQ(inserted_index, helper.insertion_index(index));
  }

  // Check that all WebStates have been restored.
  DCHECK_EQ(range.length(), static_cast<int>(restored_web_states->size()));

  // Restore the opener-opened relationship while taking into account that
  // some of the WebState have not been restored due to `scope`, and that
  // the indexes have to be adjusted.
  for (int index = range.min; index < range.max; ++index) {
    const OpenerReference ref = deserializer.GetOpenerForTabAt(index);
    if (!range.contains(ref.index)) {
      continue;
    }

    // The created WebStates are pushed in order in `restored_web_states`
    // so the opener will be at index `ref.index - range.min`.
    web::WebState* opener = (*restored_web_states)[ref.index - range.min];
    web_state_list->SetOpenerOfWebStateAt(
        helper.insertion_index(index),
        WebStateOpener(opener, ref.navigation_index));
  }
}

}  // namespace

SessionWindowIOS* SerializeWebStateList(const WebStateList* web_state_list) {
  const RemovingIndexes removing_indexes =
      GetIndexOfWebStatesToDrop(*web_state_list);

  std::map<const web::WebState*, int> index_mapping;
  for (int index = 0; index < web_state_list->count(); ++index) {
    const web::WebState* web_state = web_state_list->GetWebStateAt(index);
    index_mapping.insert(std::make_pair(web_state, index));
  }

  NSMutableArray<CRWSessionStorage*>* serialized_session =
      [[NSMutableArray alloc] init];

  for (int index = 0; index < web_state_list->count(); ++index) {
    if (removing_indexes.Contains(index)) {
      continue;
    }

    const web::WebState* web_state = web_state_list->GetWebStateAt(index);
    CRWSessionStorage* session_storage = web_state->BuildSessionStorage();
    [serialized_session addObject:session_storage];

    CRWSessionUserData* user_data = session_storage.userData;
    if (!user_data) {
      user_data = [[CRWSessionUserData alloc] init];
      session_storage.userData = user_data;
    }

    [user_data removeObjectForKey:kLegacyWebStateListPinnedStateKey];
    [user_data removeObjectForKey:kLegacyWebStateListOpenerIndexKey];
    [user_data removeObjectForKey:kLegacyWebStateListOpenerNavigationIndexKey];

    const bool is_pinned = web_state_list->IsWebStatePinnedAt(index);
    if (is_pinned) {
      [user_data setObject:@YES forKey:kLegacyWebStateListPinnedStateKey];
    }

    WebStateOpener opener = web_state_list->GetOpenerOfWebStateAt(index);
    if (opener.opener) {
      DCHECK(base::Contains(index_mapping, opener.opener));
      const int opener_index =
          removing_indexes.IndexAfterRemoval(index_mapping[opener.opener]);
      if (opener_index != WebStateList::kInvalidIndex) {
        [user_data setObject:@(opener_index)
                      forKey:kLegacyWebStateListOpenerIndexKey];
        [user_data setObject:@(opener.navigation_index)
                      forKey:kLegacyWebStateListOpenerNavigationIndexKey];
      }
    }
  }

  OrderControllerSourceFromWebStateList source(*web_state_list);
  OrderController order_controller(source);
  const int active_index = order_controller.DetermineNewActiveIndex(
      web_state_list->active_index(), std::move(removing_indexes));

  NSUInteger selectedIndex = active_index != WebStateList::kInvalidIndex
                                 ? static_cast<NSUInteger>(active_index)
                                 : static_cast<NSUInteger>(NSNotFound);

  return [[SessionWindowIOS alloc] initWithSessions:[serialized_session copy]
                                      selectedIndex:selectedIndex];
}

void SerializeWebStateList(const WebStateList& web_state_list,
                           ios::proto::WebStateListStorage& storage) {
  const RemovingIndexes removing_indexes =
      GetIndexOfWebStatesToDrop(web_state_list);

  std::map<const web::WebState*, int> index_mapping;
  for (int index = 0; index < web_state_list.count(); ++index) {
    const web::WebState* web_state = web_state_list.GetWebStateAt(index);
    index_mapping.insert(std::make_pair(web_state, index));
  }

  int removed_pinned_tabs_count = 0;
  const int pinned_tabs_count = web_state_list.pinned_tabs_count();
  for (int index = 0; index < web_state_list.count(); ++index) {
    if (removing_indexes.Contains(index)) {
      if (index < pinned_tabs_count) {
        ++removed_pinned_tabs_count;
      }
      continue;
    }

    const web::WebState* web_state = web_state_list.GetWebStateAt(index);
    ios::proto::WebStateListItemStorage& item_storage = *storage.add_items();
    item_storage.set_identifier(web_state->GetUniqueIdentifier().identifier());

    WebStateOpener opener = web_state_list.GetOpenerOfWebStateAt(index);
    if (!opener.opener) {
      continue;
    }

    DCHECK(base::Contains(index_mapping, opener.opener));
    const int opener_index =
        removing_indexes.IndexAfterRemoval(index_mapping[opener.opener]);
    if (opener_index == WebStateList::kInvalidIndex) {
      continue;
    }

    ios::proto::OpenerStorage& opener_storage = *item_storage.mutable_opener();
    opener_storage.set_index(opener_index);
    opener_storage.set_navigation_index(opener.navigation_index);
  }

  DCHECK_LE(removed_pinned_tabs_count, pinned_tabs_count);
  storage.set_pinned_item_count(pinned_tabs_count - removed_pinned_tabs_count);

  OrderControllerSourceFromWebStateList source(web_state_list);
  OrderController order_controller(source);
  const int active_index = order_controller.DetermineNewActiveIndex(
      web_state_list.active_index(), std::move(removing_indexes));
  DCHECK_LT(active_index, web_state_list.count());
  storage.set_active_index(active_index);
}

std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    SessionWindowIOS* session_window,
    SessionRestorationScope scope,
    bool enable_pinned_web_states,
    const WebStateFactory& factory) {
  std::vector<web::WebState*> restored_web_states;
  web_state_list->PerformBatchOperation(base::BindOnce(
      &DeserializeWebStateListInternal, scope, enable_pinned_web_states,
      DeserializeFromSessionWindow(session_window, factory),
      &restored_web_states));
  return restored_web_states;
}

std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    ios::proto::WebStateListStorage storage,
    SessionRestorationScope scope,
    bool enable_pinned_web_states,
    const WebStateFactoryFromProto& factory) {
  std::vector<web::WebState*> restored_web_states;
  web_state_list->PerformBatchOperation(base::BindOnce(
      &DeserializeWebStateListInternal, scope, enable_pinned_web_states,
      DeserializeFromProto(std::move(storage), factory), &restored_web_states));
  return restored_web_states;
}
