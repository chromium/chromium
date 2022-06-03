// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_map.h"

namespace blink {

class AudioParamMapIterationSource final
    : public PairIterable<String, AudioParam*>::IterationSource {
 public:
  AudioParamMapIterationSource(
      const HeapHashMap<String, Member<AudioParam>>& map) {
    for (const auto& name : map.Keys()) {
      parameter_names_.push_back(name);
      parameter_objects_.push_back(map.at(name));
    }
  }

  bool Next(ScriptState* scrip_state,
            String& key,
            AudioParam*& audio_param,
            ExceptionState&) override {
    if (current_index_ == parameter_names_.size())
      return false;
    key = parameter_names_[current_index_];
    audio_param = parameter_objects_[current_index_];
    ++current_index_;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parameter_objects_);
    PairIterable<String, AudioParam*>::IterationSource::Trace(visitor);
  }

 private:
  // For sequential iteration (e.g. Next()).
  Vector<String> parameter_names_;
  HeapVector<Member<AudioParam>> parameter_objects_;
  unsigned current_index_;
};

AudioParamMap::AudioParamMap(
    const HeapHashMap<String, Member<AudioParam>>& parameter_map)
    : parameter_map_(parameter_map) {}

PairIterable<String, AudioParam*>::IterationSource*
    AudioParamMap::StartIteration(ScriptState*, ExceptionState&) {
  return MakeGarbageCollected<AudioParamMapIterationSource>(parameter_map_);
}

bool AudioParamMap::GetMapEntry(ScriptState*,
                                const String& key,
                                AudioParam*& audio_param,
                                ExceptionState&) {
  if (parameter_map_.Contains(key)) {
    audio_param = parameter_map_.at(key);
    return true;
  }

  return false;
}

}  // namespace blink
