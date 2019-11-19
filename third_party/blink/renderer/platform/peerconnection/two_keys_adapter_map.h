// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TWO_KEYS_ADAPTER_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TWO_KEYS_ADAPTER_MAP_H_

#include <map>
#include <memory>
#include <utility>

#include "base/logging.h"

namespace blink {

// A map with up to two keys per entry. An element is inserted with a key, this
// is the primary key. A secondary key can optionally be set to the same entry
// and it may be set at a later point in time than the element was inserted. For
// lookup and erasure both keys can be used.
//
// This was designed to assist the implementation of adapter maps. The adapters
// are the glue between the blink and webrtc layer objects. The adapter maps
// keep track of which blink and webrtc layer objects have which associated
// adapter. This requires two keys per adapter entry: something that can be used
// for lookup based on a webrtc layer object and something that can be used for
// lookup based on a blink layer object. The primary key is based on the
// webrtc/blink object that was used to create the adapter and the secondary key
// is based on the resulting blink/webrtc object after the adapter has been
// initialized.
//
// TODO(crbug.com/787254): Move this class out of the Blink exposed API when
// its clients get Onion souped, and change the use of std::map below to
// WTF::HashMap.
template <typename PrimaryKey, typename SecondaryKey, typename Value>
class TwoKeysAdapterMap {
 public:
  // Maps the primary key to the value, increasing |PrimarySize| by one and
  // allowing lookup of the value based on primary key. Returns a pointer to the
  // value in the map, the pointer is valid for as long as the value is in the
  // map. There must not already exist a mapping for this primary key, in other
  // words |!FindByPrimary(primary)| must hold.
  Value* Insert(PrimaryKey primary, Value value) {
    DCHECK(entries_by_primary_.find(primary) == entries_by_primary_.end());
    auto it = entries_by_primary_
                  .insert(std::make_pair(
                      std::move(primary),
                      std::unique_ptr<Entry>(new Entry(std::move(value)))))
                  .first;
    it->second->primary_it = it;
    return &it->second->value;
  }

  // Maps the secondary key to the value mapped by the primary key, increasing
  // |SecondarySize| by one and allowing lookup of the value based on secondary
  // key.
  // There must exist a mapping for this primary key and there must not already
  // exist a mapping for this secondary key, in other words
  // |FindByPrimary(primary) && !FindBySecondary(secondary)| must hold.
  void SetSecondaryKey(const PrimaryKey& primary, SecondaryKey secondary) {
    auto it = entries_by_primary_.find(primary);
    DCHECK(it != entries_by_primary_.end());
    DCHECK(entries_by_secondary_.find(secondary) ==
           entries_by_secondary_.end());
    Entry* entry = it->second.get();
    entry->secondary_it =
        entries_by_secondary_
            .insert(std::make_pair(std::move(secondary), entry))
            .first;
  }

  // Returns a pointer to the value mapped by the primary key, or null if the
  // primary key is not mapped to any value. The pointer is valid for as long as
  // the value is in the map.
  Value* FindByPrimary(const PrimaryKey& primary) const {
    auto it = entries_by_primary_.find(primary);
    if (it == entries_by_primary_.end())
      return nullptr;
    return &it->second->value;
  }

  // Returns a pointer to the value mapped by the secondary key, or null if the
  // secondary key is not mapped to any value. The pointer is valid for as long
  // as the value is in the map.
  Value* FindBySecondary(const SecondaryKey& secondary) const {
    auto it = entries_by_secondary_.find(secondary);
    if (it == entries_by_secondary_.end())
      return nullptr;
    return &it->second->value;
  }

  // Erases the value associated with the primary key, removing the mapping of
  // of its primary and secondary key, if it had one. Returns true on removal or
  // false if there was no value associated with the primary key.
  bool EraseByPrimary(const PrimaryKey& primary) {
    auto primary_it = entries_by_primary_.find(primary);
    if (primary_it == entries_by_primary_.end())
      return false;
    if (primary_it->second->secondary_it != entries_by_secondary_.end())
      entries_by_secondary_.erase(primary_it->second->secondary_it);
    entries_by_primary_.erase(primary_it);
    return true;
  }

  // Erases the value associated with the secondary key, removing the mapping of
  // of both its primary and secondary keys. Returns true on removal or false if
  // there was no value associated with the secondary key.
  bool EraseBySecondary(const SecondaryKey& secondary) {
    auto secondary_it = entries_by_secondary_.find(secondary);
    if (secondary_it == entries_by_secondary_.end())
      return false;
    entries_by_primary_.erase(secondary_it->second->primary_it);
    entries_by_secondary_.erase(secondary_it);
    return true;
  }

  // The number of elements in the map.
  size_t PrimarySize() const { return entries_by_primary_.size(); }
  // The number of elements in the map which have secondary keys.
  size_t SecondarySize() const { return entries_by_secondary_.size(); }
  bool empty() const { return entries_by_primary_.empty(); }

 private:
  // TODO(crbug.com/787254): Move this class out of the Blink exposed API when
  // its clients get Onion souped.
  struct Entry {
    Entry(Value value) : value(std::move(value)) {}

    Value value;
    typename std::map<PrimaryKey, std::unique_ptr<Entry>>::iterator primary_it;
    typename std::map<SecondaryKey, Entry*>::iterator secondary_it;
  };

  typename std::map<PrimaryKey, std::unique_ptr<Entry>> entries_by_primary_;
  typename std::map<SecondaryKey, Entry*> entries_by_secondary_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_TWO_KEYS_ADAPTER_MAP_H_
