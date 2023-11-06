// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_loading.h"

#import <vector>

#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/raw_ref.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source.h"
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

}  // namespace

SessionStorage::SessionStorage() {
  session_metadata.set_active_index(-1);
}

SessionStorage::SessionStorage(
    ios::proto::WebStateListStorage session_metadata_arg,
    WebStateMetadataStorageMap web_state_storage_map_arg)
    : session_metadata(std::move(session_metadata_arg)),
      web_state_storage_map(std::move(web_state_storage_map_arg)) {
  DCHECK_EQ(session_metadata.items_size(),
            static_cast<int>(web_state_storage_map.size()));
}

SessionStorage::SessionStorage(SessionStorage&&) = default;

SessionStorage& SessionStorage::operator=(SessionStorage&&) = default;

SessionStorage::~SessionStorage() = default;

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

    result.set_active_index(order_controller.DetermineNewActiveIndex(
        storage.active_index(), removing_indexes));
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

    // Fix the opener index (to take into acount the closed items) or clear
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
  return result;
}

SessionStorage LoadSessionStorage(const base::FilePath& directory) {
  const base::FilePath session_metadata_file =
      directory.Append(kSessionMetadataFilename);

  // If the session metadata cannot be loaded, then the session is absent or
  // has been corrupted; return an empty session.
  ios::proto::WebStateListStorage session_metadata;
  if (!ParseProto(session_metadata_file, session_metadata)) {
    return SessionStorage();
  }

  std::vector<int> items_to_drop;
  SessionStorage::WebStateMetadataStorageMap web_state_storage_map;

  const int items_size = session_metadata.items_size();
  std::set<web::WebStateID> seen_identifiers;
  // Count the number of dropped tabs because they are duplicates, for
  // reporting.
  int duplicate_count = 0;
  for (int index = 0; index < items_size; ++index) {
    // If the item identifier is invalid, then the session has been corrupted;
    // return an empty session.
    const auto& item = session_metadata.items(index);
    if (!web::WebStateID::IsValidValue(item.identifier())) {
      return SessionStorage();
    }

    const auto web_state_id =
        web::WebStateID::FromSerializedValue(item.identifier());

    const base::FilePath web_state_dir =
        WebStateDirectory(directory, web_state_id);

    const base::FilePath web_state_metadata_file =
        web_state_dir.Append(kWebStateMetadataStorageFilename);

    // If the item metadata cannot be loaded, then the session has been
    // corrupted; return an empty session.
    web::proto::WebStateMetadataStorage metadata;
    if (!ParseProto(web_state_metadata_file, metadata)) {
      return SessionStorage();
    }

    const base::FilePath web_state_storage_file =
        web_state_dir.Append(kWebStateStorageFilename);

    // If the item storage does not exist, then the session has been
    // corrupted; return an empty session.
    if (!FileExists(web_state_storage_file)) {
      return SessionStorage();
    }

    // If the item would be empty, drop it before restoration.
    if (!metadata.navigation_item_count()) {
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

    web_state_storage_map.insert(
        std::make_pair(web_state_id, std::move(metadata)));
  }
  base::UmaHistogramCounts100("Tabs.DroppedDuplicatesCountOnSessionRestore",
                              duplicate_count);

  session_metadata = FilterItems(std::move(session_metadata),
                                 RemovingIndexes(std::move(items_to_drop)));

  return SessionStorage(std::move(session_metadata),
                        std::move(web_state_storage_map));
}

}  // namespace ios::sessions
