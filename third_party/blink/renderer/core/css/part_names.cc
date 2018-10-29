// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/part_names.h"

#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"

namespace blink {

namespace {
// Adds the names to the set.
static void AddToSet(const SpaceSplitString& strings,
                     HashSet<AtomicString>* set) {
  for (wtf_size_t i = 0; i < strings.size(); i++) {
    set->insert(strings[i]);
  }
}
}  // namespace

PartNames::PartNames(const SpaceSplitString& names) {
  AddToSet(names, &names_);
}

void PartNames::PushMap(const NamesMap& names_map) {
  pending_maps_.push_back(&names_map);
}

void PartNames::ApplyMap(const NamesMap& names_map) {
  HashSet<AtomicString> new_names;
  for (const AtomicString& name : names_) {
    if (base::Optional<SpaceSplitString> mapped_names = names_map.Get(name))
      AddToSet(mapped_names.value(), &new_names);
  }
  std::swap(names_, new_names);
}

bool PartNames::Contains(const AtomicString& name) {
  // If we have any, apply all pending maps and clear the queue.
  if (pending_maps_.size()) {
    for (const NamesMap* pending_map : pending_maps_) {
      ApplyMap(*pending_map);
    }
    pending_maps_.clear();
  }
  return names_.Contains(name);
}

size_t PartNames::size() {
  return names_.size();
}

}  // namespace blink
