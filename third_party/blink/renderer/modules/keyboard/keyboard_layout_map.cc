// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_layout_map.h"

namespace blink {

class KeyboardLayoutMapIterationSource final
    : public PairSyncIterable<KeyboardLayoutMap>::IterationSource {
 public:
  explicit KeyboardLayoutMapIterationSource(const KeyboardLayoutMap& map)
      : map_(map), iterator_(map_->Map().begin()) {}

  bool FetchNextItem(ScriptState* script_state,
                     String& map_key,
                     String& map_value,
                     ExceptionState&) override {
    if (iterator_ == map_->Map().end())
      return false;
    map_key = iterator_->key;
    map_value = iterator_->value;
    ++iterator_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    PairSyncIterable<KeyboardLayoutMap>::IterationSource::Trace(visitor);
  }

 private:
  // Needs to be kept alive while we're iterating over it.
  const Member<const KeyboardLayoutMap> map_;
  HashMap<String, String>::const_iterator iterator_;
};

KeyboardLayoutMap::KeyboardLayoutMap(const HashMap<String, String>& map)
    : layout_map_(map) {}

PairSyncIterable<KeyboardLayoutMap>::IterationSource*
KeyboardLayoutMap::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<KeyboardLayoutMapIterationSource>(*this);
}

bool KeyboardLayoutMap::GetMapEntry(ScriptState*,
                                    const String& key,
                                    String& value,
                                    ExceptionState&) {
  auto it = layout_map_.find(key);
  if (it == layout_map_.end())
    return false;

  value = it->value;
  return true;
}

}  // namespace blink
