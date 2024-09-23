// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_loading.h"

#import <vector>

#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/raw_ref.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

namespace ios::sessions {
namespace {

// A concrete implementation of OrderControllerSource that query data
// from an ios::proto::WebStateListStorage.
class OrderControllerSourceFromWebStateListStorage final
    : public OrderControllerSource {
 public:
  // Constructor taking the `session_storage` used to return the data.
  explicit OrderControllerSourceFromWebStateListStorage(
      const ios::proto::WebStateListStorage& session_metadata);

  // OrderControllerSource implementation.
  int GetCount() const final;
  int GetPinnedCount() const final;
  int GetOpenerOfItemAt(int index) const final;
  bool IsOpenerOfItemAt(int index,
                        int opener_index,
                        bool check_navigation_index) const final;
  TabGroupRange GetGroupRangeOfItemAt(int index) const final;
  std::set<int> GetCollapsedGroupIndexes() const final;

 private:
  raw_ref<const ios::proto::WebStateListStorage> session_metadata_;
};

OrderControllerSourceFromWebStateListStorage::
    OrderControllerSourceFromWebStateListStorage(
        const ios::proto::WebStateListStorage& session_metadata)
    : session_metadata_(session_metadata) {}

int OrderControllerSourceFromWebStateListStorage::GetCount() const {
  return session_metadata_->items_size();
}

int OrderControllerSourceFromWebStateListStorage::GetPinnedCount() const {
  return session_metadata_->pinned_item_count();
}

int OrderControllerSourceFromWebStateListStorage::GetOpenerOfItemAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, session_metadata_->items_size());
  const auto& item_storage = session_metadata_->items(index);
  if (!item_storage.has_opener()) {
    return WebStateList::kInvalidIndex;
  }

  return item_storage.opener().index();
}

bool OrderControllerSourceFromWebStateListStorage::IsOpenerOfItemAt(
    int index,
    int opener_index,
    bool check_navigation_index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, session_metadata_->items_size());

  // `check_navigation_index` is only used for `DetermineInsertionIndex()`
  // which should not be used, so we can assert that the parameter is false.
  DCHECK(!check_navigation_index);

  const auto& item_storage = session_metadata_->items(index);
  if (!item_storage.has_opener()) {
    return false;
  }

  return item_storage.opener().index() == opener_index;
}

TabGroupRange
OrderControllerSourceFromWebStateListStorage::GetGroupRangeOfItemAt(
    int index) const {
  for (auto& group_storage : session_metadata_->groups()) {
    const ios::proto::RangeIndex range_index = group_storage.range();
    const TabGroupRange group_range(range_index.start(), range_index.count());
    if (group_range.contains(index)) {
      return group_range;
    }
  }
  return TabGroupRange::InvalidRange();
}

std::set<int>
OrderControllerSourceFromWebStateListStorage::GetCollapsedGroupIndexes() const {
  std::set<int> collapsed_indexes;

  for (auto& group_storage : session_metadata_->groups()) {
    if (group_storage.collapsed()) {
      const ios::proto::RangeIndex range_index = group_storage.range();
      const TabGroupRange group_range(range_index.start(), range_index.count());
      collapsed_indexes.insert(group_range.begin(), group_range.end());
    }
  }
  return collapsed_indexes;
}

// Returns an ios::proto::WebStateListStorage representing an empty session.
ios::proto::WebStateListStorage EmptyWebStateListStorage() {
  ios::proto::WebStateListStorage web_state_list_storage;
  web_state_list_storage.set_active_index(-1);
  return web_state_list_storage;
}

}  // namespace

base::FilePath WebStateDirectory(const base::FilePath& directory,
                                 web::WebStateID identifier) {
  return directory.Append(base::StringPrintf("%08x", identifier.identifier()));
}

ios::proto::WebStateListStorage FilterItems(
    ios::proto::WebStateListStorage storage,
    const RemovingIndexes& removing_indexes) {
  // If there is no items to remove, return the input unmodified.
  if (removing_indexes.count() == 0) {
    return storage;
  }

  // Compute the new active index, before removing items from `storage`.
  ios::proto::WebStateListStorage result;
  {
    const OrderControllerSourceFromWebStateListStorage source(storage);
    const OrderController order_controller(source);

    result.set_active_index(removing_indexes.IndexAfterRemoval(
        order_controller.DetermineNewActiveIndex(storage.active_index(),
                                                 removing_indexes)));
  }

  const int items_size = storage.items_size();
  int pinned_item_count = storage.pinned_item_count();

  for (int index = 0; index < items_size; ++index) {
    if (removing_indexes.Contains(index)) {
      if (index < storage.pinned_item_count()) {
        DCHECK_GE(pinned_item_count, 1);
        --pinned_item_count;
      }
      continue;
    }

    // Add a new item, copying its value from the old item.
    ios::proto::WebStateListItemStorage* item_storage = result.add_items();
    item_storage->Swap(storage.mutable_items(index));

    // Fix the opener index (to take into account the closed items) or clear
    // the opener information if the opener has been closed.
    if (item_storage->has_opener()) {
      const int opener_index = item_storage->opener().index();
      if (removing_indexes.Contains(opener_index)) {
        item_storage->clear_opener();
      } else {
        item_storage->mutable_opener()->set_index(
            removing_indexes.IndexAfterRemoval(opener_index));
      }
    }
  }

  DCHECK_GE(pinned_item_count, 0);
  DCHECK_LE(pinned_item_count, result.items_size());
  result.set_pinned_item_count(pinned_item_count);

  // Create the new list of tab groups, updating the range `start` and
  // range `count` properties.
  for (auto& initial_group_storage : *storage.mutable_groups()) {
    const ios::proto::RangeIndex initial_range_index =
        initial_group_storage.range();
    const TabGroupRange initial_range(initial_range_index.start(),
                                      initial_range_index.count());
    const TabGroupRange final_range =
        removing_indexes.RangeAfterRemoval(initial_range);
    if (final_range.valid()) {
      // Add a new group, copying its value from the old group.
      ios::proto::TabGroupStorage* group_storage = result.add_groups();
      group_storage->Swap(&initial_group_storage);
      ios::proto::RangeIndex* range_index = group_storage->mutable_range();
      range_index->set_start(final_range.range_begin());
      range_index->set_count(final_range.count());
    }
  }

  return result;
}

ios::proto::WebStateListStorage LoadSessionStorage(
    const base::FilePath& directory) {
  const base::FilePath session_metadata_file =
      directory.Append(kSessionMetadataFilename);

  // If the session metadata cannot be loaded, then the session is absent or
  // has been corrupted; return an empty session.
  ios::proto::WebStateListStorage session;
  if (!ParseProto(session_metadata_file, session)) {
    return EmptyWebStateListStorage();
  }

  // Used to filter items (either duplicate or WebState with no navigations).
  std::vector<int> items_to_drop;

  // Count the number of dropped tabs because they are duplicates, for
  // reporting.
  std::set<web::WebStateID> seen_identifiers;
  int duplicate_count = 0;

  const int items_size = session.items_size();
  for (int index = 0; index < items_size; ++index) {
    // If the item identifier is invalid, then the session has been corrupted;
    // return an empty session.
    const auto& item = session.items(index);
    if (!web::WebStateID::IsValidValue(item.identifier())) {
      return EmptyWebStateListStorage();
    }

    const auto web_state_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    const base::FilePath web_state_dir =
        WebStateDirectory(directory, web_state_id);

    const base::FilePath web_state_storage_file =
        web_state_dir.Append(kWebStateStorageFilename);

    // If the item storage does not exist, then the session has been
    // corrupted; return an empty session.
    if (!FileExists(web_state_storage_file)) {
      return EmptyWebStateListStorage();
    }

    // If the item would be empty, drop it before restoration.
    if (!item.metadata().navigation_item_count()) {
      items_to_drop.push_back(index);
      continue;
    }

    // If the item is a duplicate, drop it before restoration.
    if (seen_identifiers.contains(web_state_id)) {
      items_to_drop.push_back(index);
      duplicate_count++;
      continue;
    }
    seen_identifiers.insert(web_state_id);
  }
  base::UmaHistogramCounts100("Tabs.DroppedDuplicatesCountOnSessionRestore",
                              duplicate_count);

  return FilterItems(std::move(session),
                     RemovingIndexes(std::move(items_to_drop)));
}

}  // namespace ios::sessions
