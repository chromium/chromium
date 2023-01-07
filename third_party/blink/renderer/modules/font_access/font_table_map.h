// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_TABLE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_TABLE_MAP_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/bindings/core/v8/maplike.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class ScriptState;

class BLINK_EXPORT FontTableMap final
    : public ScriptWrappable,
      public Maplike<String, IDLString, Member<Blob>, Blob> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using MapType = HeapHashMap<String, Member<Blob>>;

  explicit FontTableMap(const MapType& table_map) : table_map_(table_map) {}

  const MapType& GetHashMap() const { return table_map_; }

  // IDL attributes / methods
  uint32_t size() const { return table_map_.size(); }

  void Trace(Visitor* visitor) const override;

 private:
  PairIterable<String, IDLString, Member<Blob>, Blob>::IterationSource*
  StartIteration(ScriptState*, ExceptionState&) override;

  bool GetMapEntry(ScriptState*,
                   const String& key,
                   Member<Blob>&,
                   ExceptionState&) override;

  const MapType table_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_TABLE_MAP_H_
