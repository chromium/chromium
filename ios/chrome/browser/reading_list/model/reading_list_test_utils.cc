// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/time/default_clock.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"

namespace {

// Testing factories use RepeatingCallbacks, which can't possibly work with
// move-only arguments. Instead, let's implement copy semantics that should be
// good enough for testing purposes.
std::vector<scoped_refptr<ReadingListEntry>> CloneEntries(
    const std::vector<scoped_refptr<ReadingListEntry>>& entries) {
  std::vector<scoped_refptr<ReadingListEntry>> copied_entries;
  const base::Time now = base::Time::Now();
  for (const auto& entry : entries) {
    std::unique_ptr<reading_list::ReadingListLocal> entry_as_local =
        entry->AsReadingListLocal(now);
    copied_entries.push_back(
        ReadingListEntry::FromReadingListLocal(*entry_as_local, now));
  }
  return copied_entries;
}

}  // namespace

std::unique_ptr<KeyedService> BuildReadingListModelWithFakeStorage(
    const std::vector<scoped_refptr<ReadingListEntry>>& initial_entries,
    web::BrowserState* context) {
  auto storage = std::make_unique<FakeReadingListModelStorage>();
  base::WeakPtr<FakeReadingListModelStorage> storage_ptr = storage->AsWeakPtr();
  auto reading_list_model = std::make_unique<ReadingListModelImpl>(
      std::move(storage), syncer::StorageType::kUnspecified,
      syncer::WipeModelUponSyncDisabledBehavior::kNever,
      base::DefaultClock::GetInstance());
  // Complete the initial model load from storage.
  storage_ptr->TriggerLoadCompletion(CloneEntries(initial_entries));
  return reading_list_model;
}
