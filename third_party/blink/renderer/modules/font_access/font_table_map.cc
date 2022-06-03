// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_table_map.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

void FontTableMap::Trace(Visitor* visitor) const {
  visitor->Trace(table_map_);
  ScriptWrappable::Trace(visitor);
}

class FontTableMapIterationSource final
    : public PairIterable<String, Member<Blob>>::IterationSource {
 public:
  explicit FontTableMapIterationSource(const FontTableMap::MapType& map) {
    for (const auto& table_name : map.Keys()) {
      table_names_.push_back(table_name);
      table_data_.push_back(map.at(table_name));
    }
  }

  bool Next(ScriptState* script_state,
            String& map_key,
            Member<Blob>& map_value,
            ExceptionState&) override {
    if (current_index_ == table_names_.size())
      return false;

    map_key = table_names_[current_index_];
    map_value = table_data_[current_index_];

    ++current_index_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(table_data_);
    PairIterable<String, Member<Blob>>::IterationSource::Trace(visitor);
  }

 private:
  Vector<String> table_names_;
  HeapVector<Member<Blob>> table_data_;
  unsigned current_index_;
};

PairIterable<String, Member<Blob>>::IterationSource*
FontTableMap::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<FontTableMapIterationSource>(table_map_);
}

bool FontTableMap::GetMapEntry(ScriptState*,
                               const String& key,
                               Member<Blob>& value,
                               ExceptionState&) {
  if (table_map_.Contains(key)) {
    value = table_map_.at(key);
    return true;
  }

  return false;
}

}  // namespace blink
