// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/storage_interest_group.h"

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {

StorageInterestGroup::StorageInterestGroup() = default;
StorageInterestGroup::StorageInterestGroup(StorageInterestGroup&&) = default;
StorageInterestGroup::~StorageInterestGroup() = default;

std::ostream& operator<<(std::ostream& out,
                         const StorageInterestGroup::KAnonymityData& kanon) {
  return out << "KAnonymityData[key=`" << kanon.key
             << "`, is_k_anonymous=" << kanon.is_k_anonymous
             << ", last_updated=`" << kanon.last_updated << "`]";
}

}  // namespace content
