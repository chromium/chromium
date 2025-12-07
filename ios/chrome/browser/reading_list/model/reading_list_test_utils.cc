// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/default_clock.h"
#include "components/reading_list/core/fake_reading_list_model_storage.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/sync/base/storage_type.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Constructs a ReadingListModel instance that uses a fake persistency
// layer. The constructed model starts loaded and initially contains
// the entries provided in `initial_entries`..
std::unique_ptr<KeyedService> BuildReadingListModelWithFakeStorage(
    std::vector<scoped_refptr<ReadingListEntry>> initial_entries,
    ProfileIOS* profile) {
  auto storage = std::make_unique<FakeReadingListModelStorage>();
  base::WeakPtr<FakeReadingListModelStorage> storage_ptr = storage->AsWeakPtr();
  auto reading_list_model = std::make_unique<ReadingListModelImpl>(
      std::move(storage), syncer::StorageType::kUnspecified,
      syncer::WipeModelUponSyncDisabledBehavior::kNever,
      base::DefaultClock::GetInstance());
  // Complete the initial model load from storage.
  storage_ptr->TriggerLoadCompletion(std::move(initial_entries));
  return reading_list_model;
}

}  // namespace

ProfileKeyedServiceFactoryIOS::TestingFactory
ReadingListModelTestingFactoryWithFakeStorage(
    std::vector<scoped_refptr<ReadingListEntry>> initial_entries) {
  return base::BindOnce(&BuildReadingListModelWithFakeStorage,
                        std::move(initial_entries));
}
