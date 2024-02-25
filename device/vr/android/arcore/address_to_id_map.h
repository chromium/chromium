// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_ARCORE_ADDRESS_TO_ID_MAP_H_
#define DEVICE_VR_ANDROID_ARCORE_ADDRESS_TO_ID_MAP_H_

#include <limits>
#include <optional>
#include <unordered_map>

#include "base/check.h"

namespace device {

// Wrapper class used to generate an Id for a given address. Allows looking up
// the Id for an address at a later time, or removing the mapping when desired.
// Ids generated will be monotonically increasing from 1, and are suitable to
// be passed over mojo. The Ids should be directly exposable from blink if
// desired.
// Note that IdType must be constructable from a uint64_t, and should most often
// be a base::IdTypeU64 type.
template <typename IdType>
class AddressToIdMap {
 public:
  // Helper struct to provide a cleaner return interface than a std::pair for
  // CreateOrGetId.
  struct CreateOrGetIdResult {
    // The found or newly created Id corresponding to the supplied address.
    IdType id;

    // Whether or not the above Id was newly created (true), or found (false).
    bool created;

    CreateOrGetIdResult(IdType id, bool created) : id(id), created(created) {}
  };

  // Retrieves or creates an id for the corresponding address.
  CreateOrGetIdResult CreateOrGetId(void* address) {
    auto it = address_to_id_.find(address);
    if (it != address_to_id_.end()) {
      return {it->second, false};
    }

    CHECK(next_id_ != std::numeric_limits<uint64_t>::max())
        << "preventing ID overflow";

    uint64_t current_id = next_id_;
    next_id_++;
    address_to_id_.emplace(address, current_id);

    return {IdType(current_id), true};
  }

  // Gets the id for the corresponding address, if it's available.
  std::optional<IdType> GetId(void* address) const {
    auto it = address_to_id_.find(address);
    if (it == address_to_id_.end()) {
      return std::nullopt;
    }

    return it->second;
  }

  // Used to "erase" a particular id->address mapping, such that lookup methods
  // for the given address will fail. This will result in a new id being
  // generated if the address is passed into CreateOrGetId.
  template <class Predicate>
  size_t EraseIf(Predicate pred) {
    return std::erase_if(address_to_id_, pred);
  }

 private:
  // The HashMaps used in blink do not allow Ids that evaluate to 0. Thus, we
  // start generating Ids from 1, so that the first IdType does not cause any
  // issues in blink.
  uint64_t next_id_ = 1;
  std::unordered_map<void*, IdType> address_to_id_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_ARCORE_ADDRESS_TO_ID_MAP_H_
