// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_PORT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_PORT_MAP_H_

#include "third_party/blink/renderer/bindings/core/v8/maplike.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <typename T>
class MIDIPortMap : public ScriptWrappable, public Maplike<String, T*> {
 public:
  explicit MIDIPortMap(const HeapVector<Member<T>>& entries)
      : entries_(entries) {}

  // IDL attributes / methods
  uint32_t size() const { return entries_.size(); }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(entries_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  // We use HeapVector here to keep the entry order.
  using Entries = HeapVector<Member<T>>;
  using IteratorType = typename Entries::const_iterator;

  typename PairIterable<String, T*>::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override {
    return new MapIterationSource(this, entries_.begin(), entries_.end());
  }

  bool GetMapEntry(ScriptState*,
                   const String& key,
                   T*& value,
                   ExceptionState&) override {
    // FIXME: This function is not O(1). Perhaps it's OK because in typical
    // cases not so many ports are connected.
    for (const auto& p : entries_) {
      if (key == p->id()) {
        value = p;
        return true;
      }
    }
    return false;
  }

  // Note: This template class relies on the fact that m_map.m_entries will
  // never be modified once it is created.
  class MapIterationSource final
      : public PairIterable<String, T*>::IterationSource {
   public:
    MapIterationSource(MIDIPortMap<T>* map,
                       IteratorType iterator,
                       IteratorType end)
        : map_(map), iterator_(iterator), end_(end) {}

    bool Next(ScriptState* script_state,
              String& key,
              T*& value,
              ExceptionState&) override {
      if (iterator_ == end_)
        return false;
      key = (*iterator_)->id();
      value = *iterator_;
      ++iterator_;
      return true;
    }

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(map_);
      PairIterable<String, T*>::IterationSource::Trace(visitor);
    }

   private:
    // m_map is stored just for keeping it alive. It needs to be kept
    // alive while JavaScript holds the iterator to it.
    const Member<const MIDIPortMap<T>> map_;
    IteratorType iterator_;
    const IteratorType end_;
  };

  const Entries entries_;
};

}  // namespace blink

#endif
