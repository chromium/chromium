// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"

namespace blink {

XRInputSource* XRInputSourceArray::AnonymousIndexedGetter(
    unsigned index) const {
  if (index >= input_sources_.size())
    return nullptr;

  auto it = input_sources_.Values().begin();

  // HeapHashMap's iterators don't expose a generic + operator.  We're ensuring
  // that this won't be past the end with the size check above.
  for (unsigned i = 0; i < index; i++) {
    ++it;
  }

  return *(it.Get());
}

XRInputSource* XRInputSourceArray::operator[](unsigned index) const {
  DCHECK(index < length());
  return AnonymousIndexedGetter(index);
}

XRInputSource* XRInputSourceArray::GetWithSourceId(uint32_t source_id) {
  auto it = input_sources_.find(source_id);
  if (it != input_sources_.end())
    return it->value.Get();
  return nullptr;
}

void XRInputSourceArray::RemoveWithSourceId(uint32_t source_id) {
  auto it = input_sources_.find(source_id);
  if (it != input_sources_.end())
    input_sources_.erase(it);
}

void XRInputSourceArray::SetWithSourceId(uint32_t source_id,
                                         XRInputSource* input_source) {
  input_sources_.Set(source_id, input_source);
}

void XRInputSourceArray::Trace(Visitor* visitor) const {
  visitor->Trace(input_sources_);
  ScriptWrappable::Trace(visitor);
}
}  // namespace blink
