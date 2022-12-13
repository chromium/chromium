// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_map.h"

namespace blink {

class AudioParamMapIterationSource final
    : public PairSyncIterable<AudioParamMap>::IterationSource {
 public:
  explicit AudioParamMapIterationSource(
      const HeapHashMap<String, Member<AudioParam>>& map) {
    parameter_names_.ReserveInitialCapacity(map.size());
    parameter_objects_.ReserveInitialCapacity(map.size());
    for (const auto& item : map) {
      parameter_names_.push_back(item.key);
      parameter_objects_.push_back(item.value);
    }
  }

  bool FetchNextItem(ScriptState* scrip_state,
                     String& key,
                     AudioParam*& audio_param,
                     ExceptionState&) override {
    if (current_index_ >= parameter_names_.size()) {
      return false;
    }
    key = parameter_names_[current_index_];
    audio_param = parameter_objects_[current_index_];
    ++current_index_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parameter_objects_);
    PairSyncIterable<AudioParamMap>::IterationSource::Trace(visitor);
  }

 private:
  Vector<String> parameter_names_;
  HeapVector<Member<AudioParam>> parameter_objects_;
  unsigned current_index_;
};

AudioParamMap::AudioParamMap(
    const HeapHashMap<String, Member<AudioParam>>& parameter_map)
    : parameter_map_(parameter_map) {}

PairSyncIterable<AudioParamMap>::IterationSource*
AudioParamMap::CreateIterationSource(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<AudioParamMapIterationSource>(parameter_map_);
}

bool AudioParamMap::GetMapEntry(ScriptState*,
                                const String& key,
                                AudioParam*& value,
                                ExceptionState&) {
  auto it = parameter_map_.find(key);
  if (it == parameter_map_.end())
    return false;
  value = it->value;
  return true;
}

}  // namespace blink
