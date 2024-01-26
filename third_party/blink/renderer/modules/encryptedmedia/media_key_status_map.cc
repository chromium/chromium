// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_status_map.h"

#include <algorithm>
#include <limits>

#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Represents the key ID and associated status.
class MediaKeyStatusMap::MapEntry final
    : public GarbageCollected<MediaKeyStatusMap::MapEntry> {
 public:
  MapEntry(WebData key_id, const String& status)
      : key_id_(DOMArrayBuffer::Create(scoped_refptr<SharedBuffer>(key_id))),
        status_(status) {}
  virtual ~MapEntry() = default;

  DOMArrayBuffer* KeyId() const { return key_id_.Get(); }

  const String& Status() const { return status_; }

  static bool CompareLessThan(MapEntry* a, MapEntry* b) {
    // Compare the keyIds of 2 different MapEntries. Assume that |a| and |b|
    // are not null, but the keyId() may be. KeyIds are compared byte
    // by byte.
    DCHECK(a);
    DCHECK(b);

    // Handle null cases first (which shouldn't happen).
    //    |aKeyId|    |bKeyId|     result
    //      null        null         == (false)
    //      null      not-null       <  (true)
    //    not-null      null         >  (false)
    if (!a->KeyId() || !b->KeyId())
      return b->KeyId();

    // Compare the bytes.
    int result =
        memcmp(a->KeyId()->Data(), b->KeyId()->Data(),
               std::min(a->KeyId()->ByteLength(), b->KeyId()->ByteLength()));
    if (result != 0)
      return result < 0;

    // KeyIds are equal to the shared length, so the shorter string is <.
    DCHECK_NE(a->KeyId()->ByteLength(), b->KeyId()->ByteLength());
    return a->KeyId()->ByteLength() < b->KeyId()->ByteLength();
  }

  virtual void Trace(Visitor* visitor) const { visitor->Trace(key_id_); }

 private:
  const Member<DOMArrayBuffer> key_id_;
  const String status_;
};

// Represents an Iterator that loops through the set of MapEntrys.
class MapIterationSource final
    : public PairSyncIterable<MediaKeyStatusMap>::IterationSource {
 public:
  MapIterationSource(MediaKeyStatusMap* map) : map_(map), current_(0) {}

  bool FetchNextItem(ScriptState* script_state,
                     V8BufferSource*& key,
                     V8MediaKeyStatus& value,
                     ExceptionState&) override {
    // This simply advances an index and returns the next value if any,
    // so if the iterated object is mutated values may be skipped.
    if (current_ >= map_->size())
      return false;

    const auto& entry = map_->at(current_++);
    key = MakeGarbageCollected<V8BufferSource>(entry.KeyId());
    value = entry.Status();
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(map_);
    PairSyncIterable<MediaKeyStatusMap>::IterationSource::Trace(visitor);
  }

 private:
  // m_map is stored just for keeping it alive. It needs to be kept
  // alive while JavaScript holds the iterator to it.
  const Member<const MediaKeyStatusMap> map_;
  uint32_t current_;
};

void MediaKeyStatusMap::Clear() {
  entries_.clear();
}

void MediaKeyStatusMap::AddEntry(WebData key_id, const String& status) {
  // Insert new entry into sorted list.
  auto* entry = MakeGarbageCollected<MapEntry>(key_id, status);
  uint32_t index = 0;
  while (index < entries_.size() &&
         MapEntry::CompareLessThan(entries_[index], entry))
    ++index;
  entries_.insert(index, entry);
}

const MediaKeyStatusMap::MapEntry& MediaKeyStatusMap::at(uint32_t index) const {
  DCHECK_LT(index, entries_.size());
  return *entries_.at(index);
}

uint32_t MediaKeyStatusMap::IndexOf(const DOMArrayPiece& key) const {
  for (uint32_t index = 0; index < entries_.size(); ++index) {
    auto* const current = entries_.at(index)->KeyId();
    if (key == *current)
      return index;
  }

  // Not found, so return an index outside the valid range. The caller
  // must ensure this value is not exposed outside this class.
  return std::numeric_limits<uint32_t>::max();
}

bool MediaKeyStatusMap::has(
    const V8BufferSource* key_id
) {
  uint32_t index = IndexOf(key_id);
  return index < entries_.size();
}

ScriptValue MediaKeyStatusMap::get(ScriptState* script_state,
                                   const V8BufferSource* key_id
) {
  uint32_t index = IndexOf(key_id);
  v8::Isolate* isolate = script_state->GetIsolate();
  if (index >= entries_.size()) {
    return ScriptValue(isolate, v8::Undefined(isolate));
  }
  return ScriptValue(isolate, V8String(isolate, at(index).Status()));
}

MediaKeyStatusMap::IterationSource* MediaKeyStatusMap::CreateIterationSource(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<MapIterationSource>(this);
}

void MediaKeyStatusMap::Trace(Visitor* visitor) const {
  visitor->Trace(entries_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
