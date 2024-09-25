// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_state_list_serialization.h"

#import <stdint.h>

#import <algorithm>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/session_id_generator.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/sessions/model/features.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/proto/tab_group.pb.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/tab_group_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_state.h"

using tab_group_util::DeserializedGroup;

namespace {

// Whether a particular web state should be kept or filtered out. This checks
// for empty tabs (i.e. without navigation), and duplicates.
bool ShouldKeepWebState(const web::WebState* web_state,
                        const std::set<web::WebStateID>& seen_identifiers,
                        int& duplicate_count) {
  if (seen_identifiers.contains(web_state->GetUniqueIdentifier())) {
    duplicate_count++;
    return false;
  }

  if (web_state->GetNavigationItemCount()) {
    // WebState has navigation history, keep.
    return true;
  }

  if (web_state->IsRealized()) {
    const web::NavigationManager* manager = web_state->GetNavigationManager();
    if (manager->GetPendingItem()) {
      // WebState has navigation pending, keep.
      return true;
    }
  }

  return false;
}

// Creates a RemovingIndexes that records the indexes of the WebStates that
// should not be saved.
//
// Some WebState may have no back/forward history. This can happen for
// multiple reason (one is when opening a new tab on a slow network session,
// and terminating the app before the navigation can commit, another is when
// WKWebView intercepts a new tab navigation to an app navigation; there may
// be other cases).
// Some WebState might have become inconsistent and have the same identifier.
// This remove method removes the duplicates and keeps the first occurrence.
RemovingIndexes GetIndexOfWebStatesToDrop(const WebStateList& web_state_list) {
  std::vector<int> web_state_to_skip_indexes;
  std::set<web::WebStateID> seen_identifiers;
  // Count the number of dropped tabs because they are duplicates, for
  // reporting.
  int duplicate_count = 0;
  for (int index = 0; index < web_state_list.count(); ++index) {
    const web::WebState* web_state = web_state_list.GetWebStateAt(index);
    if (ShouldKeepWebState(web_state, seen_identifiers, duplicate_count)) {
      seen_identifiers.insert(web_state->GetUniqueIdentifier());
      continue;
    }

    web_state_to_skip_indexes.push_back(index);
  }
  base::UmaHistogramCounts100("Tabs.DroppedDuplicatesCountOnSessionSave",
                              duplicate_count);

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

  // Returns the total number of tab groups in the serialized state.
  virtual int GetTabGroupsCount() const = 0;

  // Returns the deserisalized tab group at `index`.
  virtual DeserializedGroup GetDeserializedGroupAt(int index) const = 0;
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
  int GetTabGroupsCount() const override;
  DeserializedGroup GetDeserializedGroupAt(int index) const override;

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

int DeserializeFromSessionWindow::GetTabGroupsCount() const {
  return session_window_.tabGroups.count;
}

DeserializedGroup DeserializeFromSessionWindow::GetDeserializedGroupAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(session_window_.tabGroups.count));
  return tab_group_util::FromSerializedValue(session_window_.tabGroups[index]);
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
  int GetTabGroupsCount() const override;
  DeserializedGroup GetDeserializedGroupAt(int index) const override;

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
      web::WebStateID::FromSerializedValue(item_storage.identifier()),
      item_storage.metadata());
}

int DeserializeFromProto::GetTabGroupsCount() const {
  return storage_.groups_size();
}

DeserializedGroup DeserializeFromProto::GetDeserializedGroupAt(
    int index) const {
  return tab_group_util::FromSerializedValue(storage_.groups(index));
}

// Helper function that deserialize into a WebStateList using a deserializer.
// Used by the two implementation of `DeserializeWebStateList(...)`.
std::vector<web::WebState*> DeserializeWebStateListInternal(
    WebStateList* web_state_list,
    bool enable_pinned_web_states,
    bool enable_tab_groups,
    const Deserializer& deserializer) {
  DCHECK(web_state_list);
  DCHECK(web_state_list->empty());

  // Lock the WebStateList.
  WebStateList::ScopedBatchOperation lock =
      web_state_list->StartBatchOperation();

  int restored_pinned_tabs_count = 0;
  const int restored_tabs_count = deserializer.GetRestoredTabsCount();
  if (enable_pinned_web_states) {
    restored_pinned_tabs_count = deserializer.GetRestoredPinnedTabsCount();
  }

  // If the restoration range is empty, then there is nothing to do. This
  // can happen if the storage is empt.
  DCHECK_GE(restored_tabs_count, 0);
  if (restored_tabs_count == 0) {
    return {};
  }

  // Exactly `restored_tabs_count` WebState should be deserialized.
  std::vector<web::WebState*> restored_web_states;
  restored_web_states.reserve(restored_tabs_count);

  // Get the index of the active item according to storage. Used to mark
  // the WebState as active during the insertion, if restored.
  const int active_index = deserializer.GetActiveIndex();

  SessionID max_identifier = SessionID::InvalidValue();

  // Restore all items directly at their correct position in the WebStateList.
  // The opener-opened relationship is not restored yet, as some WebState may
  // have an opener that is stored after them.
  for (int index = 0; index < restored_tabs_count; ++index) {
    std::unique_ptr<web::WebState> web_state = deserializer.RestoreTabAt(index);
    restored_web_states.push_back(web_state.get());  // Store pointer to item.

    if (base::FeatureList::IsEnabled(
            session::features::kSessionRestorationSessionIDCheck)) {
      web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
      CHECK(web_state_id.valid(), base::NotFatalUntil::M125);
      if (!max_identifier.is_valid() ||
          max_identifier.id() < web_state_id.identifier()) {
        max_identifier = web_state_id.ToSessionID();
      }
    }

    const int inserted_index = web_state_list->InsertWebState(
        std::move(web_state), WebStateList::InsertionParams::AtIndex(index)
                                  .Pinned(index < restored_pinned_tabs_count)
                                  .Activate(index == active_index));

    DCHECK_EQ(inserted_index, index);
  }

  if (base::FeatureList::IsEnabled(
          session::features::kSessionRestorationSessionIDCheck)) {
    sessions::SessionIdGenerator::GetInstance()->SetHighestRestoredID(
        max_identifier);
  }

  // Check that all WebStates have been restored.
  DCHECK_EQ(restored_tabs_count, static_cast<int>(restored_web_states.size()));

  // Restore the opener-opened relationship.
  for (int index = 0; index < restored_tabs_count; ++index) {
    const OpenerReference ref = deserializer.GetOpenerForTabAt(index);
    if (ref.index < 0 || ref.index >= restored_tabs_count) {
      continue;
    }

    // The created WebStates are pushed in order in `restored_web_states`
    // so the opener will be at index `ref.index`.
    web::WebState* opener = restored_web_states[ref.index];
    web_state_list->SetOpenerOfWebStateAt(
        index, WebStateOpener(opener, ref.navigation_index));
  }

  // Deserialize and create tab groups.
  if (enable_tab_groups) {
    const int tab_group_count = deserializer.GetTabGroupsCount();
    std::set<tab_groups::TabGroupId> tab_group_ids;
    for (int i = 0; i < tab_group_count; ++i) {
      DeserializedGroup group = deserializer.GetDeserializedGroupAt(i);
      if (group.range_start < restored_pinned_tabs_count ||
          group.range_start > restored_tabs_count) {
        continue;
      }
      if (group.range_count <= 0 ||
          group.range_start + group.range_count > restored_tabs_count) {
        continue;
      }

      tab_groups::TabGroupId tab_group_id = group.tab_group_id;
      if (!tab_group_id.is_empty()) {
        // Check that there is no duplicate  of `tab_group_id`.
        // It is improbable, but not impossible, for this to occur.
        if (tab_group_ids.contains(tab_group_id)) {
          base::debug::DumpWithoutCrashing();
          continue;
        }
        tab_group_ids.insert(tab_group_id);
      } else {
        tab_group_id = tab_groups::TabGroupId::GenerateNew();
      }

      web_state_list->CreateGroup(
          TabGroupRange(group.range_start, group.range_count).AsSet(),
          group.visual_data, tab_group_id);
    }
  }

  return restored_web_states;
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
  const int active_index = removing_indexes.IndexAfterRemoval(
      order_controller.DetermineNewActiveIndex(web_state_list->active_index(),
                                               removing_indexes));

  NSUInteger selectedIndex = active_index != WebStateList::kInvalidIndex
                                 ? static_cast<NSUInteger>(active_index)
                                 : static_cast<NSUInteger>(NSNotFound);

  NSMutableArray<SessionTabGroup*>* serialized_groups =
      [[NSMutableArray alloc] init];
  for (const TabGroup* group : web_state_list->GetGroups()) {
    const TabGroupRange initial_range = group->range();
    const TabGroupRange final_range =
        removing_indexes.RangeAfterRemoval(initial_range);
    if (final_range.valid()) {
      SessionTabGroup* serialized_group = [[SessionTabGroup alloc]
          initWithRangeStart:final_range.range_begin()
                  rangeCount:final_range.count()
                       title:base::SysUTF16ToNSString(
                                 group->visual_data().title())
                     colorId:static_cast<NSInteger>(
                                 group->visual_data().color())
              collapsedState:group->visual_data().is_collapsed()
                  tabGroupId:group->tab_group_id()];
      [serialized_groups addObject:serialized_group];
    }
  }

  return [[SessionWindowIOS alloc] initWithSessions:[serialized_session copy]
                                          tabGroups:[serialized_groups copy]
                                      selectedIndex:selectedIndex];
}

void SerializeWebStateList(const WebStateList& web_state_list,
                           const WebStateMetadataMap& metadata_map,
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
    const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();

    ios::proto::WebStateListItemStorage& item_storage = *storage.add_items();
    item_storage.set_identifier(web_state_id.identifier());

    DCHECK(base::Contains(metadata_map, web_state_id));
    auto iter = metadata_map.find(web_state_id);
    *item_storage.mutable_metadata() = iter->second;

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

  for (const TabGroup* group : web_state_list.GetGroups()) {
    const TabGroupRange initial_range = group->range();
    const TabGroupRange final_range =
        removing_indexes.RangeAfterRemoval(initial_range);
    if (final_range.valid()) {
      ios::proto::TabGroupStorage& group_storage = *storage.add_groups();
      ios::proto::RangeIndex& range = *group_storage.mutable_range();

      range.set_start(final_range.range_begin());
      range.set_count(final_range.count());

      group_storage.set_title(base::UTF16ToUTF8(group->visual_data().title()));
      group_storage.set_color(
          tab_group_util::ColorForStorage(group->visual_data().color()));
      group_storage.set_collapsed(group->visual_data().is_collapsed());
      tab_group_util::TabGroupIdForStorage(
          group->tab_group_id(), *group_storage.mutable_tab_group_id());
    }
  }

  DCHECK_LE(removed_pinned_tabs_count, pinned_tabs_count);
  storage.set_pinned_item_count(pinned_tabs_count - removed_pinned_tabs_count);

  OrderControllerSourceFromWebStateList source(web_state_list);
  OrderController order_controller(source);
  const int active_index = removing_indexes.IndexAfterRemoval(
      order_controller.DetermineNewActiveIndex(web_state_list.active_index(),
                                               removing_indexes));
  DCHECK_LT(active_index, web_state_list.count());
  storage.set_active_index(active_index);
}

std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    SessionWindowIOS* session_window,
    bool enable_pinned_web_states,
    bool enable_tab_groups,
    const WebStateFactory& factory) {
  return DeserializeWebStateListInternal(
      web_state_list, enable_pinned_web_states, enable_tab_groups,
      DeserializeFromSessionWindow(session_window, factory));
}

std::vector<web::WebState*> DeserializeWebStateList(
    WebStateList* web_state_list,
    ios::proto::WebStateListStorage storage,
    bool enable_pinned_web_states,
    bool enable_tab_groups,
    const WebStateFactoryFromProto& factory) {
  return DeserializeWebStateListInternal(
      web_state_list, enable_pinned_web_states, enable_tab_groups,
      DeserializeFromProto(std::move(storage), factory));
}
