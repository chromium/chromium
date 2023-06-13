// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class PartRoot;

// Implementation of the Part class, which is part of the DOM Parts API.
// This is the base class for all Part types, and it does not have a JS-public
// constructor.
class CORE_EXPORT Part : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Part() = default;
  Part(const Part&) = delete;
  ~Part() override = default;

  // Part API
  virtual PartRoot* root() const = 0;

  // TODO(crbug.com/1453291) Implement this method.
  HeapVector<String> metadata() const { return HeapVector<String>(); }

  void disconnect() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_
